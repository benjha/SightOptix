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
#include "globals.h"
#include "../header/sutil.h"
#include "../header/Arcball.h"
#include "sightOptixStructs.h"

//#define POST_PROCESSING

class cPNGEncoder;

using namespace optix;

#define NUM_PARTICLES_PER_GROUP 		1000000
#define GROUP_SIZE			512

class cMouseHandler;
class cKeyboardHandler;


class cOptixRenderer
{
public:
	enum
	{
		ENTRY_POINT_MAIN_SHADING=0,
		ENTRY_POINT_FLOAT4_TO_COLOR,
		ENTRY_POINT_FLOAT4_TO_DENOISED_COLOR
	};
	cOptixRenderer		( bool shareBuffer	);
	~cOptixRenderer	(	);

	// initialize Optix contex, framebuffer size and camera
	// position based on min/max values  and allocate
	// number of steps
	void init (int width, int height, float *min, float *max, size_t steps=1);
	void init (int width, int height,
               std::vector<float> *pos, float *min, float *max );

	// initialize array of spheres and creates Optix scene graph
	// organized as:
	//
	//  |---GeometryGroup
	//      |---GeometryInstance
	//          |---Geometry
	//
	void loadSpheres ( size_t nSpheres, size_t group,
                      float *pos, float *color, float *radii );
	// store i-th time step, updating Optix's scene graph to"
	//
	//  |---TimeStep (GeometryGroup)
	//      |---GeometryGroup
	//          |---GeometryInstance
	//              |---Geometry
	//
	void addTimeStep ();
	// creates Optix's root to complete the hierarchy created in
	// loadSpheres method:
	//
	//  Group
	//  |---TimeStep (GeometryGroup)
	//      |---GeometryGroup
	//          |---GeometryInstance
	//              |---Geometry
	//
	void genOptixRoot ();
	// validate context and launch the renderer
	void launch ();
	bool displayProgressive ( unsigned char *pixels	);
	void display ( unsigned char *pixels	);
	void setMouseHandler ( cMouseHandler *mouseH );
	void setKeyboardHandler ( cKeyboardHandler *keyHandler );
	void getPixels ( unsigned char *img	);
	void *getGPUFrameBufferPtr () { return m_bufferPtr; };

private:
	void updateView ();
	void updateCamera ();
	void resetAccumulation ();
	void createContextProgressive ();
	void createContext ();
	void createMaterial ();
	void createInstance ();
	void setBufferIds ( const std::vector<Buffer>& buffers,
                        Buffer top_level_buffer );
	void createGeometry ( std::vector<float> *pos, float *min, float *max );
	void createGeometry ( std::vector<float> *pos );
	void setupPostprocessing ();
	void onKeyboardEvent ();

	int	m_width, m_height;
	int	m_ao_sample_mult;
	unsigned int m_frameAccum, m_numGroups, m_numPartPerGroup;
	float m_hfov, m_ratio;
	bool m_shareBuffer; // controls if Buffer will be shared with cuda
	void* m_bufferPtr; // Buffer pointer for CUDA
	Context m_context;
	Material m_material;
	Material m_materialTransparent;
	Geometry *m_sphere;
	//
	Geometry m_cube[4];
	cMouseHandler *m_mouseH;
	cKeyboardHandler *m_keyboardHandler;
	Arcball m_arcBall;
	float3 m_eye, m_lookAt, m_up, m_U, m_V, m_W;
	Matrix4x4 m_rotate;
	Buffer m_outBuffer, m_streamBuffer, m_posBuffer, m_accumBuffer;
	Aabb m_aabb;

	cPNGEncoder *m_pngEncoder;
	// True if Optix context was created
	bool contextCreated;
	// controls for auto saving
	bool m_autosave;
	bool m_denoise;
	bool m_denoiserEnabled;
	unsigned int m_renderPassCounter;
	unsigned int m_numRenderSteps;

	// Optix hierarchy Root
	optix::Group m_root;
	// should I keep track of all of this or only GeometryGroups?
	// list for user defined Optix buffer (sightSphereColor)
	std::vector<RTbuffer> m_vBuffer[NUM_GROUPS];
	// Optix geometry
	std::vector<optix::Geometry> m_vGeometry[NUM_GROUPS];
	// Optix geometry instance
	std::vector<optix::GeometryInstance> m_GeoInstance[NUM_GROUPS];
	// Optix geometry groups
	std::vector<optix::GeometryGroup> m_vGeoGroup;
	// GeometryGroup for Time step
	std::vector<optix::Selector> m_vTimeStep;



#ifdef POST_PROCESSING
	Buffer				m_denoisedBuffer;
	PostprocessingStage m_tonemapStage;
	PostprocessingStage m_denoiserStage;
	CommandList 		m_clDenoiser;
#endif

};


#endif // __OPTIX_PARTICLES_RENDERER_H__

