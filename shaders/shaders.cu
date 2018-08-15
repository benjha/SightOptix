
/*
 * Copyright (c) 2008 - 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and proprietary
 * rights in and to this software, related documentation and any modifications thereto.
 * Any use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation is strictly
 * prohibited.
 *
 * TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, THIS SOFTWARE IS PROVIDED *AS IS*
 * AND NVIDIA AND ITS SUPPLIERS DISCLAIM ALL WARRANTIES, EITHER EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  IN NO EVENT SHALL NVIDIA OR ITS SUPPLIERS BE LIABLE FOR ANY
 * SPECIAL, INCIDENTAL, INDIRECT, OR CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT
 * LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF
 * BUSINESS INFORMATION, OR ANY OTHER PECUNIARY LOSS) ARISING OUT OF THE USE OF OR
 * INABILITY TO USE THIS SOFTWARE, EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES
 */

#include <optix_world.h>
#include <optixu/optixu_math_namespace.h>
#include "helpers.h"
#include "random.h"
using namespace optix;


struct BasicLight
{
#if defined(__cplusplus)
  typedef optix::float3 float3;
#endif
  float3 pos;
  float3 color;
  int    casts_shadow;
  int    padding;      // make this structure 32 bytes -- powers of two are your friend!
};

struct PerRayData_radiance
{
  float3 result;
  float3 albedo;
  float3 normal;
  float importance;
  int depth;
};

struct PerRayData_shadow
{
  float3 attenuation;
};


rtDeclareVariable(float3, 		bg_color, , );
rtDeclareVariable(float3,       eye, , );
rtDeclareVariable(float3,       U, , );
rtDeclareVariable(float3,       V, , );
rtDeclareVariable(float3,       W, , );
rtDeclareVariable(float4,       bad_color, , );
rtDeclareVariable(float,        scene_epsilon, , );
rtDeclareVariable(rtObject,		top_object, , );
rtDeclareVariable(unsigned int,	radiance_ray_type, , );
rtDeclareVariable(unsigned int,	shadow_ray_type, , );
rtDeclareVariable(float3, 		shading_color, attribute shading_color, );


rtDeclareVariable(uint2, 		launch_index, rtLaunchIndex, );
rtDeclareVariable(uint2, 		launch_dim,   rtLaunchDim, );
rtDeclareVariable(float, 		time_view_scale, , ) = 1e-6f;
rtDeclareVariable(PerRayData_radiance, prd_radiance, rtPayload, );

// for antialiasing
rtBuffer<unsigned int, 2>        rnd_seeds;
rtDeclareVariable(unsigned int,	 frame, , );
rtDeclareVariable(float,         jitter_factor, ,) = 0.0f;

rtBuffer<float4, 2>              output_buffer;
rtBuffer<float4, 2>				 denoised_buffer;
rtBuffer<uchar4, 2>				 bufferToEncode;
rtBuffer<float4, 2>              accum_buffer;
rtBuffer<float4, 2>              albedo_buffer;
rtBuffer<float4, 2>              normal_buffer;



//#define TIME_VIEW

RT_PROGRAM void pinhole_camera()
{
#ifdef TIME_VIEW
	clock_t t0 = clock();
#endif
	size_t2 screen = output_buffer.size();
	// Subpixel jitter: send the ray through a different position inside the pixel each time,
	// to provide antialiasing.
	unsigned int seed = rot_seed( rnd_seeds[ launch_index ], frame );
	float2 subpixel_jitter = make_float2(rnd( seed ) - 0.5f, rnd( seed ) - 0.5f) * jitter_factor;
	float2 d = (make_float2(launch_index) + subpixel_jitter) / make_float2(screen) * 2.f - 1.f;
	float3 ray_origin = eye;
	float3 ray_direction = normalize(d.x*U + d.y*V + W);

    float3 albedo = make_float3(0.0f);
    float3 normal = make_float3(0.0f);

	optix::Ray ray = optix::make_Ray(ray_origin, ray_direction, radiance_ray_type, scene_epsilon, RT_DEFAULT_MAX );

	PerRayData_radiance prd;
    prd.result = make_float3(0.0f);
	prd.importance = 1.f;
	prd.depth = 0;

	rtTrace(top_object, ray, prd);

#ifdef TIME_VIEW
	clock_t t1 = clock();

	float expected_fps   = 1.0f;
	float pixel_time     = ( t1 - t0 ) * time_view_scale * expected_fps;
	output_buffer[launch_index] = make_color( make_float3(  pixel_time ) );
#else
	float4 acc_val = accum_buffer[launch_index];
	if( frame > 1 ){
		acc_val = lerp( acc_val, make_float4( prd.result, 0.f), 1.0f / static_cast<float>( frame+1 ) );

	}
	else
		acc_val = make_float4(prd.result, 0.f);

	output_buffer[launch_index] = acc_val;
	accum_buffer[launch_index] = acc_val;
	albedo_buffer[launch_index] = make_float4(prd.albedo, 1.0f);
	normal_buffer[launch_index] = make_float4(prd.normal,1.0f);

#endif
}

RT_PROGRAM void exception()
{
  const unsigned int code = rtGetExceptionCode();
  rtPrintf( "Caught exception 0x%X at launch index (%d,%d)\n", code, launch_index.x, launch_index.y );
  output_buffer[launch_index] = bad_color;
}

RT_PROGRAM void exception2()
{
  const unsigned int code = rtGetExceptionCode();
  rtPrintf( "Caught exception 0x%X at launch index (%d,%d)\n", code, launch_index.x, launch_index.y );
  denoised_buffer[launch_index] = bad_color;
}


RT_PROGRAM void miss()
{
	prd_radiance.result = bg_color;

	if (prd_radiance.depth==0)
	{
		prd_radiance.albedo = bg_color;
		prd_radiance.normal = make_float3(0.0f,0.0f,0.0f);
	}
}
// no denoising
RT_PROGRAM void float4TOcolor ()
{
	bufferToEncode[launch_index] = make_color (make_float3(output_buffer[launch_index]));
}

// when denoising is used
RT_PROGRAM void float4TOcolorDenoisedBuffer ()
{
	bufferToEncode[launch_index] = make_color (make_float3(denoised_buffer[launch_index]));
}


// AO
//=======================================================================================
//


struct PerRayData_occlusion
{
  float occlusion;
};

rtDeclareVariable(PerRayData_occlusion, prd_occlusion, rtPayload, );

rtDeclareVariable(float,       occlusion_distance, , );
rtDeclareVariable(int,         sqrt_occlusion_samples, , );

rtDeclareVariable(optix::Ray,	ray,          		rtCurrentRay, );
rtDeclareVariable(float,      	t_hit,        		rtIntersectionDistance, );
rtDeclareVariable(unsigned int, subframe_idx, 		rtSubframeIndex, );
rtDeclareVariable(float3,		geometric_normal, 	attribute geometric_normal, );
rtDeclareVariable(float3, 		shading_normal, 	attribute shading_normal, );

RT_PROGRAM void closest_hit_radiance_AO()
{
  float3 phit    = ray.origin + t_hit * ray.direction;

  float3 world_shading_normal   = normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, shading_normal));
  float3 world_geometric_normal = normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, geometric_normal));
  float3 ffnormal = faceforward(world_shading_normal, -ray.direction, world_geometric_normal);

  optix::Onb onb(ffnormal);

  unsigned int seed = rot_seed(rnd_seeds[launch_index], frame + subframe_idx);

  float       result           = 0.0f;
  const float inv_sqrt_samples = 1.0f / float(sqrt_occlusion_samples);
  for( int i=0; i<sqrt_occlusion_samples; ++i ) {
    for( int j=0; j<sqrt_occlusion_samples; ++j ) {

      PerRayData_occlusion prd_occ;
      prd_occ.occlusion = 0.0f;

      // Stratify samples via simple jitterring
      float u1 = (float(i) + rnd( seed ) )*inv_sqrt_samples;
      float u2 = (float(j) + rnd( seed ) )*inv_sqrt_samples;

      float3 dir;
      optix::cosine_sample_hemisphere( u1, u2, dir );
      onb.inverse_transform( dir );

      optix::Ray occlusion_ray = optix::make_Ray( phit, dir, 1, scene_epsilon,
                                                  occlusion_distance );
      rtTrace( top_object, occlusion_ray, prd_occ );

      result += 1.0f-prd_occ.occlusion;
    }
  }

  result /= (float)(sqrt_occlusion_samples*sqrt_occlusion_samples);

  prd_radiance.result = make_float3(result) * shading_color;
}

RT_PROGRAM void any_hit_occlusion_AO()
{
  prd_occlusion.occlusion = 1.0f;

  rtTerminateRay();
}


// phong Illumination
//=======================================================================================
//

rtDeclareVariable(int,              max_depth, , );
rtDeclareVariable(float3,           ambient_light_color, , );


rtDeclareVariable(rtObject,			top_shadower, , );

rtDeclareVariable(PerRayData_radiance, prd, rtPayload, );
rtDeclareVariable(PerRayData_shadow,   prd_shadow, rtPayload, );
rtDeclareVariable(float3,       Ka, , );
rtDeclareVariable(float3,       Kd, , );
rtDeclareVariable(float3,       Ks, , );
rtDeclareVariable(float3,       Kr, , );
rtDeclareVariable(float,        phong_exp, , );



rtBuffer<BasicLight>                 lights;

static __device__ void phongShadowed()
{
  // this material is opaque, so it fully attenuates all shadow rays
  prd_shadow.attenuation = optix::make_float3(0.0f);
  rtTerminateRay();
}

static
__device__ void phongShade( float3 p_Kd,
                            float3 p_Ka,
                            float3 p_Ks,
                            float3 p_Kr,
                            float  p_phong_exp,
                            float3 p_normal )
{
  float3 hit_point = ray.origin + t_hit * ray.direction;

  // ambient contribution
  float3 result = p_Ka * ambient_light_color;

  // compute direct lighting
  unsigned int num_lights = lights.size();
  for(int i = 0; i < num_lights; ++i) {
    BasicLight light = lights[i];
    float Ldist = optix::length(light.pos - hit_point);
    float3 L = optix::normalize(light.pos - hit_point);
    float nDl = optix::dot( p_normal, L);



    // cast shadow ray
    float3 light_attenuation = make_float3(static_cast<float>( nDl > 0.0f ));
    if ( nDl > 0.0f && light.casts_shadow ) {
      PerRayData_shadow shadow_prd;
      shadow_prd.attenuation = make_float3(1.0f);
      optix::Ray shadow_ray = optix::make_Ray( hit_point, L, shadow_ray_type, scene_epsilon, Ldist );
      rtTrace(top_shadower, shadow_ray, shadow_prd);
      light_attenuation = shadow_prd.attenuation;
    }

    // If not completely shadowed, light the hit point
    if( fmaxf(light_attenuation) > 0.0f ) {
      float3 Lc = light.color * light_attenuation;

      result += p_Kd * nDl * Lc;

      float3 H = optix::normalize(L - ray.direction);
      float nDh = optix::dot( p_normal, H );
      if(nDh > 0) {
        float power = pow(nDh, p_phong_exp);
        result += p_Ks * power * Lc;
      }
    }
  }

  if( fmaxf( p_Kr ) > 0 ) {

    // ray tree attenuation
    PerRayData_radiance new_prd;
    new_prd.importance = prd.importance * optix::luminance( p_Kr );
    new_prd.depth = prd.depth + 1;

    // reflection ray
    if( new_prd.importance >= 0.01f && new_prd.depth <= max_depth) {
      float3 R = optix::reflect( ray.direction, p_normal );
      optix::Ray refl_ray = optix::make_Ray( hit_point, R, radiance_ray_type, scene_epsilon, RT_DEFAULT_MAX );
      rtTrace(top_object, refl_ray, new_prd);
      result += p_Kr * new_prd.result;
    }
  }

  // pass the color back up the tree
  prd.result = result * shading_color;
}



RT_PROGRAM void any_hit_shadow()
{
  phongShadowed();
}


RT_PROGRAM void closest_hit_radiance()
{
  float3 world_shading_normal   = normalize( rtTransformNormal( RT_OBJECT_TO_WORLD, shading_normal ) );
  float3 world_geometric_normal = normalize( rtTransformNormal( RT_OBJECT_TO_WORLD, geometric_normal ) );
  float3 ffnormal = faceforward( world_shading_normal, -ray.direction, world_geometric_normal );
  phongShade( Kd, Ka, Ks, Kr, phong_exp, ffnormal );
  //prd.result = shading_color;
}


RT_PROGRAM void closest_hit_radiance_AO_phong()
{
  float3 phit    = ray.origin + t_hit * ray.direction;

  float3 world_shading_normal   = normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, shading_normal));
  float3 world_geometric_normal = normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, geometric_normal));
  float3 ffnormal = faceforward(world_shading_normal, -ray.direction, world_geometric_normal);

  optix::Onb onb(ffnormal);

  // The albedo buffer should contain an approximation of the overall surface albedo (i.e. a single
  // color value approximating the ratio of irradiance reflected to the irradiance received over the
  // hemisphere). This can be approximated for very simple materials by using the diffuse color of
  // the first hit.
  if (prd_radiance.depth == 0)
  {
	  prd_radiance.albedo = shading_color;
	  prd_radiance.normal = ffnormal;
  }

  unsigned int seed = rot_seed(rnd_seeds[launch_index], frame + subframe_idx);

  float       result           = 0.0f;
  const float inv_sqrt_samples = 1.0f / float(sqrt_occlusion_samples);
  for( int i=0; i<sqrt_occlusion_samples; ++i ) {
    for( int j=0; j<sqrt_occlusion_samples; ++j ) {

      PerRayData_occlusion prd_occ;
      prd_occ.occlusion = 0.0f;

      // Stratify samples via simple jitterring
      float u1 = (float(i) + rnd( seed ) )*inv_sqrt_samples;
      float u2 = (float(j) + rnd( seed ) )*inv_sqrt_samples;

      float3 dir;
      optix::cosine_sample_hemisphere( u1, u2, dir );
      onb.inverse_transform( dir );

      optix::Ray occlusion_ray = optix::make_Ray( phit, dir, 1, scene_epsilon,
                                                  occlusion_distance );
      rtTrace( top_object, occlusion_ray, prd_occ );

      result += 1.0f-prd_occ.occlusion;
    }
  }

  result /= (float)(sqrt_occlusion_samples*sqrt_occlusion_samples);

  // Phong
  phongShade( Kd, Ka, Ks, Kr, phong_exp, ffnormal );

  // Phong result in prd.result
  prd_radiance.result = make_float3(result);

  prd_radiance.result.x *= prd.result.x;
  prd_radiance.result.y *= prd.result.y;
  prd_radiance.result.z *= prd.result.z;
}



