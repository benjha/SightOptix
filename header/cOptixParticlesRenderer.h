/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 */

#ifndef		__OPTIX_PARTICLES_RENDERER_H__
#define 	__OPTIX_PARTICLES_RENDERER_H__

#include <iostream>
#include <cstdlib>
#include <vector>
#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_aabb_namespace.h>
#include "../header/sutil.h"
#include "../header/Arcball.h"

using namespace optix;

#define NUM_PARTICLES_PER_GROUP 		1000*1000
#define GROUP_SIZE			512

class cMouseHandler;

class cOptixParticlesRenderer
{
public:
						cOptixParticlesRenderer		( 	);
						~cOptixParticlesRenderer	(	);

	void				init						( int width, int height,
													  std::vector<float> *pos, float *min, float *max );
	bool				displayProgressive			( unsigned char *pixels	);
	void				display						( unsigned char *pixels	);
	void				setMouseHandler				( cMouseHandler *mouseH );
	unsigned char*		getPixels					(	);

private:
	void				updateView					(	);
	void				updateCamera				(	);
	void 				resetAccumulation			(	);
	void				createContextProgressive	(	);
	void				createContext				(	);
	void				createMaterial				(	);
	void				createInstance				(	);
	void 				setBufferIds				( const std::vector<Buffer>& buffers,
	                   	   	   	   	   	   	   	   	  Buffer top_level_buffer );
	void				createGeometry 			( std::vector<float> *pos, float *min, float *max );

	int					m_width, m_height;
	int					m_ao_sample_mult;
	unsigned int		m_frame, m_numGroups, m_numPartPerGroup;
	float				m_hfov, m_ratio;
	Context 			m_context;
	Material			m_material;
	Material			m_materialTransparent;
	Geometry			*m_sphere;
	Geometry			m_cube[4];
	cMouseHandler*		m_mouseH;
	Arcball				m_arcBall;
	float3				m_eye, m_lookAt, m_up, m_U, m_V, m_W;
	Matrix4x4    		m_rotate;
	Buffer				m_outBuffer, m_streamBuffer, m_posBuffer, m_accumBuffer;
	Aabb				m_aabb;

};


#endif // __OPTIX_PARTICLES_RENDERER_H__

