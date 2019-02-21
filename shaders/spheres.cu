/* 
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <optix_world.h>
#include "../header/sightOptixStructs.h"
//#define	COLOR_LOOK_UP

// How to compile cu files for optix
//  /usr/local/cuda-8.0/bin/nvcc -I../frameserver/header -I../frameserver/communications -I../../../Programs/NVIDIA-OptiX-SDK-5.0.1-linux64/include -O3 -std=c++11 -gencode arch=compute_35,code=sm_35 -gencode -gencode arch=compute_70,code=compute_70 -gencode arch=compute_50,code=sm_50 -ptx -m64 spheres.cu


using namespace optix;

rtBuffer<float4>	particle_buffer;
rtBuffer<sightSphereColor> sphere_buffer;

#ifndef COLOR_LOOK_UP
	rtBuffer<float4>	color_buffer;
#endif

//rtDeclareVariable(float4,  sphere, , );
rtDeclareVariable(float3, geometric_normal, attribute geometric_normal, ); 
rtDeclareVariable(float3, shading_normal, attribute shading_normal, );
rtDeclareVariable(float3, shading_color, attribute shading_color, );


rtDeclareVariable(optix::Ray, ray, rtCurrentRay, );

#ifdef COLOR_LOOK_UP
static __device__
float3 colorLookUp (float idx)
{
	if (idx==0.0f)
	{
		return make_float3(1.0f, 0.0f, 0.0f);
	}
	else if (idx==1.0f)
	{
		return make_float3(0.0f, 1.0f, 0.0f);
	}
	else if (idx==2.0f)
	{
		return make_float3(1.0f, 1.0f, 0.0f);
	}
	else if (idx==3.0f)
	{
		return make_float3(0.0f, 1.0f, 1.0f);
	}
	return make_float3(0.0f, 0.0f, 1.0f);
}
#endif

template<bool use_robust_method>
static __device__
void intersect_sphere(int primIdx)
{
	const float4 center4=sphere_buffer[primIdx].center;
	const float3 color=make_float3(sphere_buffer[primIdx].color);
	const float3 center=make_float3 (center4);
	const float radius = center4.w;

#ifdef COLOR_LOOK_UP
	float 			colorIdx = lookUp.w;
#endif

	float3 O = ray.origin - center;
	float3 D = ray.direction;

	float b = dot(O, D);
	float c = dot(O, O)-radius*radius;
	float disc = b*b-c;

	if(disc > 0.0f)
	{
		float sdisc = sqrtf(disc);
		float root1 = (-b - sdisc);
		bool do_refine = false;
		float root11 = 0.0f;

		if(use_robust_method && fabsf(root1) > 10.f * radius)
		{
			do_refine = true;
		}
		if(do_refine)
		{
			// refine root1
			float3 O1 = O + root1 * ray.direction;
			b = dot(O1, D);
			c = dot(O1, O1) - radius*radius;
			disc = b*b - c;

			if(disc > 0.0f)
			{
				sdisc = sqrtf(disc);
				root11 = (-b - sdisc);
			}
		}
		bool check_second = true;
		if( rtPotentialIntersection( root1 + root11 ) )
		{
			shading_normal 	= geometric_normal = (O + (root1 + root11)*D)/radius;
#ifdef COLOR_LOOK_UP
			shading_color	= colorLookUp((int)colorIdx%4);
#else
			shading_color=color;
#endif
			//shading_color	= make_float3(1.0f, 0.0f, 0.0f);


			if(rtReportIntersection(0))
				check_second = false;
		}
		/*
		if(check_second)
		{
			float root2 = (-b + sdisc) + (do_refine ? root1 : 0);
			if( rtPotentialIntersection( root2 ) )
			{
				shading_normal = geometric_normal = (O + root2*D)/radius;
#ifdef COLOR_LOOK_UP
			shading_color	= colorLookUp((int)colorIdx%4);
#else
			shading_color	= make_float3 (color_buffer[primIdx]);
#endif

				rtReportIntersection(0);
			}
		}
		*/
	}
}


RT_PROGRAM void sphere_array_intersect(int primIdx)
{

	const float4	lookUp 	= particle_buffer[primIdx];
	const float3	center 	= make_float3 (lookUp);
	const float		radius 	= lookUp.w;

	float3 V = center - ray.origin;
	float b = dot(V, ray.direction);
	float disc = b*b + radius*radius - dot(V, V);
	if (disc > 0.0f) {
		disc = sqrtf(disc);

//#define FASTONESIDEDSPHERES 1
#if defined(FASTONESIDEDSPHERES)
	// only calculate the nearest intersection, for speed
    float t1 = b - disc;
		if (rtPotentialIntersection(t1))
		{
			shading_normal = geometric_normal = (t1*ray.direction - V) / radius;
	    	shading_color	= make_float3 (color_buffer[primIdx]);
			rtReportIntersection(0);
		}
 #else
	float t2 = b + disc;
    if (rtPotentialIntersection(t2))
    {
    	shading_normal = geometric_normal = (t2*ray.direction - V) / radius;
    	//shading_color	= make_float3 (color_buffer[primIdx]);

    	rtReportIntersection(0);
    }
    float t1 = b - disc;
    if (rtPotentialIntersection(t1))
    {
    	shading_normal = geometric_normal = (t1*ray.direction - V) / radius;
    	//shading_color	= make_float3 (color_buffer[primIdx]);
    	rtReportIntersection(0);
    }
#endif
	}
}

RT_PROGRAM void intersect(int primIdx)
{
	//rtPrintf( "primIdx %d", primIdx);
	intersect_sphere<false>( primIdx );

}


RT_PROGRAM void robust_intersect(int primIdx)
{
	intersect_sphere<true>( primIdx );
}


RT_PROGRAM void bounds (int primIdx, float result[6])
{
	//rtPrintf( "primIdx %d", primIdx);
//	const int 		idx		= index_buffer[primIdx];
	const float4 center4=sphere_buffer[primIdx].center;
	const float3 center=make_float3 (center4);
	const float3 vRadius=make_float3 (center4.w);

	optix::Aabb* aabb = (optix::Aabb*)result;

	if ( vRadius.x > 0.0f && !isinf(vRadius.x))
	{
	    aabb->m_min = center - vRadius;
	    aabb->m_max = center + vRadius;
	}
	else
	{
		aabb->invalidate();
	}

}



// 1D buffer
rtBuffer<rtBufferId<float4> > posBufferIds;
rtBuffer<rtBufferId<float4> > colBufferIds;



RT_PROGRAM void robust_intersect_BoB(int primIdx)
{
	unsigned int i;
	for ( i=0; i<posBufferIds.size();++i )
	{
	    // Grab a reference to the nested buffer so we dont need to perform
	    // the buffer lookup multiple times
	    rtBufferId<float4, 1>& posBuffer = posBufferIds[i];

		intersect_sphere<true>( primIdx );
	}
}


