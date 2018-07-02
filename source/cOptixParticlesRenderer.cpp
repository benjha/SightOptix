/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 */

#include <string.h>
#include "../frameserver/header/cMouseEventHandler.h"
#include "../frameserver/header/cKeyboardHandler.h"
#include "../frameserver/header/cPNGEncoder.h"
#include "../header/cOptixParticlesRenderer.h"
#include "../header/Arcball.h"
#include "../header/DeviceMemoryLogger.h"
#include "../header/cColorTable.h"
#include "../header/loaders.h"
#include "../header/ImageLoader.h"
#include "../shaders/commonStructs.h"
#include "../shaders/random.h"

#include <cuda_profiler_api.h>

//#define STREAM_PROGRESSIVE

cOptixParticlesRenderer::cOptixParticlesRenderer( )
{
	m_width 	= 1280;
	m_height 	= 720;
	m_frameAccum = 0;
	m_context	= 0;
	m_mouseH	= 0;
	m_ao_sample_mult = 1;
	m_hfov		= 60.0f;
	m_ratio		= 1.0f;
	m_eye		= (float3){0.0f, 0.0f, 100.0f };
	m_lookAt	= (float3){0.0f, 0.0f, 0.0f };
	m_up		= (float3){ 0.0f, 1.0f, 0.0f };
    m_rotate  	= Matrix4x4::identity();

    m_pngEncoder 		= 0;
	m_autosave			= false;
	m_renderPassCounter	= 0;
	m_numRenderSteps	= 10;
	m_denoiserEnabled	= false;
}

cOptixParticlesRenderer::~cOptixParticlesRenderer ( )
{
#ifdef POST_PROCESSING
	m_clDenoiser->destroy();
	m_tonemapStage->destroy();
	m_denoiserStage->destroy();
#endif
	m_context->destroy();
	delete [] m_sphere;
	delete m_pngEncoder;

}
//
//=======================================================================================
//
void cOptixParticlesRenderer::init ( int width, int height, std::vector<float> *pos, float *min, float *max)
{
	m_width 	= width;
	m_height 	= height;
	m_hfov    	= 60.0f;

	// Setting camera view according to min / max position values:
	//glm::vec3 pos (  max[0]+min[0] / 2.0, max[1]+min[1] / 2.0, max[2]+min[2] / 2.0  );

	m_aabb.set( (float3) {min[0], min[1], min[2]}, (float3){max[0], max[1], max[2]} );
    const float max_dim = fmaxf(m_aabb.extent(1), m_aabb.extent(2)); // max of y, z components
    m_eye    = m_aabb.center() + make_float3( 0.0f, 0.0f, max_dim*5.0f );
    m_lookAt = m_aabb.center();


	int bitrate;
	int fps;
	char streamFormat[128];

	try
	{
#ifdef STREAM_PROGRESSIVE
		createContextProgressive();
#else
		createContext	( );
#endif
		DeviceMemoryLogger::logDeviceDescription(m_context, std::cout);
		// Setup state
		createGeometry ( pos, min, max);
		createMaterial( );
		createInstance( );

#ifdef STREAM_PROGRESSIVE
		m_streamBuffer->getAttribute(RT_BUFFER_ATTRIBUTE_STREAM_BITRATE, sizeof(int), &bitrate );
		m_streamBuffer->getAttribute(RT_BUFFER_ATTRIBUTE_STREAM_FPS, sizeof(int), &fps );
		m_streamBuffer->getAttribute(RT_BUFFER_ATTRIBUTE_STREAM_FORMAT, sizeof(streamFormat), &streamFormat);
		std::cout << "Bitrate " << bitrate << " fps " << fps << " format " << streamFormat << std::endl;
#endif
		m_context->validate();

#ifdef STREAM_PROGRESSIVE
		m_context->launchProgressive( 0, m_width, m_height, 100 );
#else
		DeviceMemoryLogger::logCurrentMemoryUsage(m_context, std::cout);
		resetAccumulation ();
		m_context->launch ( 0, 0 );
		DeviceMemoryLogger::logCurrentMemoryUsage(m_context, std::cout);
#endif

	} catch( Exception& e ){
	  std::cout << e.getErrorString().c_str() << std::endl;
	exit(1);
	}

#ifdef POST_PROCESSING
	setupPostprocessing ();
#endif

	m_pngEncoder = new cPNGEncoder ();
	m_pngEncoder->setImageParams(m_width, m_height);
	if (!(m_pngEncoder->initEncoder()))
	{
		std::cout << "Warning: Renderer's PNG Encoder failed at initialization \n";
	}
	std::cout << "\nRenderer's PNG Encoder initialized\n";



}
//
//=======================================================================================
//
void cOptixParticlesRenderer::setMouseHandler (cMouseHandler *mouseH)
{
	m_mouseH = mouseH;
}
//
//=======================================================================================
//
void cOptixParticlesRenderer::setKeyboardHandler (cKeyboardHandler *keyHandler)
{
	m_keyboardHandler = keyHandler;
}
//
//=======================================================================================
//
void cOptixParticlesRenderer::updateCamera()
{
	sutil::calculateCameraVariables(
			  m_eye, m_lookAt, m_up, m_hfov, m_ratio,
			  m_U, m_V, m_W );

    const Matrix4x4 frame = Matrix4x4::fromBasis(
            normalize( m_U ),
            normalize( m_V ),
            normalize( -m_W ),
            m_lookAt);
    const Matrix4x4 frame_inv = frame.inverse();
    // Apply camera rotation twice to match old SDK behavior
    const Matrix4x4 trans   = frame*m_rotate*m_rotate*frame_inv;

    m_eye    = make_float3( trans*make_float4( m_eye,    1.0f ) );
    m_lookAt = make_float3( trans*make_float4( m_lookAt, 1.0f ) );
    m_up     = make_float3( trans*make_float4( m_up,     0.0f ) );

	sutil::calculateCameraVariables(
			  m_eye, m_lookAt, m_up, m_hfov, m_ratio,
			  m_U, m_V, m_W );

    m_rotate = Matrix4x4::identity();

    m_context["eye"]->setFloat( m_eye );
    m_context["U"  ]->setFloat( m_U );
    m_context["V"  ]->setFloat( m_V );
    m_context["W"  ]->setFloat( m_W );
}
//
//=======================================================================================
//
void cOptixParticlesRenderer::updateView ( )
{
	static int lastX=0, lastY=0;
	switch (m_mouseH->getButton())
	{
	case cMouseHandler::LEFT_BUTTON:
		if (m_mouseH->getState()==cMouseHandler::DOWN)
		{
	        const float2 from = { static_cast<float>(lastX),
	                              static_cast<float>(lastY) };
	        const float2 to   = { static_cast<float>( m_mouseH->getX()),
	                              static_cast<float>( m_mouseH->getY()) };

	        const float2 a = { from.x / m_width, from.y / m_height };
	        const float2 b = { to.x   / m_width, to.y   / m_height };

	        m_rotate = m_arcBall.rotate( b, a );
	        updateCamera ();
			resetAccumulation();
		}
		break;
	case cMouseHandler::RIGHT_BUTTON:
		if (m_mouseH->getState()==cMouseHandler::DOWN)
		{
			const float dx = static_cast<float>( m_mouseH->getX() - lastX ) /
							 static_cast<float>( m_width );
			const float dy = static_cast<float>( m_mouseH->getY() - lastY ) /
							 static_cast<float>( m_height );
			const float dmax = fabsf( dx ) > fabs( dy ) ? dx : dy;
			const float scale = fminf( dmax, 10.0f );
			m_eye = m_eye + (m_lookAt - m_eye)*scale;

			updateCamera ();
			resetAccumulation();
		}
		break;
	case cMouseHandler::MIDDLE_BUTTON:
		break;
	}

	lastX = m_mouseH->getX();
	lastY = m_mouseH->getY();

}
//
//=======================================================================================
//
bool cOptixParticlesRenderer::displayProgressive (unsigned char *pixels)
{

	bool streamReady = false;
    unsigned int subframe_count, max_subframes;
	streamReady = m_streamBuffer->getProgressiveUpdateReady (subframe_count, max_subframes);
	//std::cout << "subframe: " << subframe_count << " max_subframes: " << max_subframes;
	//std::cout << "stream ready: " << streamReady;
	//sutil::displayBufferPPM( pixels, m_context["output_buffer"]->getBuffer()->get() );

	if (streamReady)
	{
		sutil::displayStreamBuffer(pixels, m_streamBuffer->get(), m_width, m_height);
	}

	if (m_mouseH->refreshed())
	{
		updateView ();
		m_mouseH->refresh(false);
		m_context->launchProgressive( 0, m_width, m_height, 100 );
	}
	//	std::cout << "Launch\n";

    //static unsigned frame_count = 0;
    //sutil::displayFps( frame_count++ );

	return streamReady;
}
//
//=======================================================================================
//
void cOptixParticlesRenderer::display (unsigned char *pixels)
{
	double fpsexpave = 0.0;
	static unsigned frame_count = 0;
	if (m_mouseH->refreshed())
	{
		updateView ();
		m_mouseH->refresh(false);

	}
	if ( m_keyboardHandler->refreshed())
	{
		onKeyboardEvent ( );
		m_keyboardHandler->refresh( false );
	}
	//else
	//{
		// Use more AO samples if the camera is not moving.
		// Do this above launch to avoid overweighting the first frame
		//m_context["occlusion_distance"]->setFloat(1000.0f);
		//m_context["sqrt_diffuse_samples"]->setInt( 3 );
	  //  m_context["sqrt_occlusion_samples"]->setInt(3);
	//}

	if (!m_denoiserEnabled)
	{
		m_context->launch( 0, m_width, m_height);
		sutil::displayBuffer(pixels, m_context["output_buffer"]->getBuffer()->get());
	}
	if (m_denoiserEnabled && m_denoise)
	{
		m_clDenoiser->execute();
		sutil::displayBuffer(pixels, m_denoisedBuffer->get());
		// check albedo:
		//sutil::displayBuffer(pixels, m_context["albedo_buffer"]->getBuffer()->get());
		// check normals:
		//sutil::displayBuffer(pixels, m_context["normal_buffer"]->getBuffer()->get());
	}


	m_context["frame"]->setUint( m_frameAccum++ );

	if (m_autosave==true)
	{
		m_renderPassCounter++;
	}
	if (m_renderPassCounter==m_numRenderSteps)
	{
		char date[128];
		time_t rawtime;
		struct tm * timeinfo;
		time(&rawtime);
		timeinfo = localtime (&rawtime);
		strftime (date,sizeof(date),"%Y-%m-%d_%OH_%OM_%OS",timeinfo);
		std::stringstream filename;
		filename << "Sight_AutoSave_" << date << ".png";
		m_pngEncoder->savePNG(filename.str(), pixels);
		std::cout << filename.str().data() << " saved!\n";
		m_renderPassCounter = 0;
		m_autosave = false;
		m_denoise = true;
	}


	// The frame number is used as part of the random seed.
	//m_context["frame"]->setInt( m_frame );
    sutil::displayFps( ++frame_count );

}
//
//=======================================================================================
//
void cOptixParticlesRenderer::createContext ( 	)
{
	// Set up context
	m_context = Context::create();
	m_context->setRayTypeCount( 2 );
	m_context->setEntryPointCount( 1 );


#ifndef POST_PROCESSING
	// Output buffer
	Buffer buffer =  m_context->createBuffer( RT_BUFFER_OUTPUT, RT_FORMAT_FLOAT4, m_width, m_height );
	m_context["output_buffer"]->set( buffer );

	// Accumulation buffer
	Buffer accum_buffer = m_context->createBuffer( RT_BUFFER_INPUT_OUTPUT | RT_BUFFER_GPU_LOCAL, RT_FORMAT_FLOAT4, m_width, m_height);
	m_context["accum_buffer"]->set( accum_buffer );
	//resetAccumulation();
#else
	// Output buffer
	Buffer buffer =  m_context->createBuffer( RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_FLOAT4, m_width, m_height );
	m_context["output_buffer"]->set( buffer );

	// Accumulation buffer
	Buffer accum_buffer = m_context->createBuffer( RT_BUFFER_INPUT_OUTPUT | RT_BUFFER_GPU_LOCAL, RT_FORMAT_FLOAT4, m_width, m_height);
	m_context["accum_buffer"]->set( accum_buffer );
	resetAccumulation();

	Buffer tonemappedBuffer = m_context->createBuffer(RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_FLOAT4, m_width, m_height);
	m_context["tonemapped_buffer"]->set(tonemappedBuffer);

    Buffer albedoBuffer = m_context->createBuffer(RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_FLOAT4, m_width, m_height);
    m_context["albedo_buffer"]->set(albedoBuffer);

    Buffer normalBuffer = m_context->createBuffer(RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_FLOAT4, m_width, m_height);
    m_context["normal_buffer"]->set(normalBuffer);

    m_denoisedBuffer = m_context->createBuffer( RT_BUFFER_OUTPUT, RT_FORMAT_FLOAT4, m_width, m_height );

#endif

	std::string ptx_path( "shaders/shaders.ptx" );

	// Ray generation program
	Program ray_gen_program = m_context->createProgramFromPTXFile( ptx_path, "pinhole_camera" );
	m_context->setRayGenerationProgram( 0, ray_gen_program );

	// Exception program
	Program exception_program = m_context->createProgramFromPTXFile( ptx_path, "exception" );
	m_context->setExceptionProgram( 0, exception_program );
	m_context["bad_color"]->setFloat( 1.0f, 0.0f, 1.0f, 0.0f );

	// Miss program
	m_context->setMissProgram( 0, m_context->createProgramFromPTXFile( ptx_path, "miss" ) );
	m_context["bg_color"]->setFloat(0.95f, 0.95f, 0.95f );

	m_ratio = static_cast<float>(m_width) / static_cast<float>(m_height);
	sutil::calculateCameraVariables(
		  m_eye, m_lookAt, m_up, m_hfov, m_ratio,
		  m_U, m_V, m_W );

	m_context["radiance_ray_type"	]->setUint( 0u );
	m_context["shadow_ray_type"  	]->setUint( 1u );
	m_context["scene_epsilon"    	]->setFloat( 1.0f);
	m_context["eye"					]->setFloat( m_eye );
	m_context["U"					]->setFloat( m_U );
	m_context["V"					]->setFloat( m_V );
	m_context["W"					]->setFloat( m_W );
	m_context["max_depth"			]->setInt( 1 );

	m_context["jitter_factor"		]->setFloat( 1.0f);
	m_context["frame"				]->setUint( 0 );


	// Random seed buffer
	Buffer seed = m_context->createBuffer( RT_BUFFER_INPUT , RT_FORMAT_UNSIGNED_INT, m_width, m_height);
	unsigned int* seeds = static_cast<unsigned int*>( seed->map() );
	fillRandBuffer (seeds, m_width*m_height);
	seed->unmap();
	m_context["rnd_seeds"]->setBuffer(seed);

}
//
//=======================================================================================
//
void cOptixParticlesRenderer::createContextProgressive (	)
{
	// Set up context
	m_context = Context::create();

	m_context->setEntryPointCount( 1 );
	m_context->setRayTypeCount( 3 );
	RTsize size = 256;
	m_context->setStackSize(size);
	// Output buffer
	Variable output_buffer = m_context["output_buffer"];
	Buffer buffer = m_context->createBuffer( RT_BUFFER_OUTPUT, RT_FORMAT_FLOAT4, m_width, m_height);
	output_buffer->set(buffer);
	// stream buffer
	m_streamBuffer = m_context->createBuffer (RT_BUFFER_PROGRESSIVE_STREAM, RT_FORMAT_UNSIGNED_BYTE4, m_width, m_height);
	m_streamBuffer->bindProgressiveStream(buffer);
	int bitrate = 500000;
	m_streamBuffer->setAttribute(RT_BUFFER_ATTRIBUTE_STREAM_BITRATE, sizeof(int), &bitrate  );

	// Ray generation program
	std::string ptx_path( "shaders/accum_camera.ptx" );
	Program ray_gen_program = m_context->createProgramFromPTXFile( ptx_path, "pinhole_camera" );
	m_context->setRayGenerationProgram( 0, ray_gen_program );

	// Exception program
	Program exception_program = m_context->createProgramFromPTXFile( ptx_path, "exception" );
	m_context->setExceptionProgram( 0, exception_program );
	m_context["bad_color"]->setFloat( 1.0f, 0.0f, 1.0f );

	// Miss program
	ptx_path = "shaders/constantbg.ptx";
	m_context->setMissProgram( 0, m_context->createProgramFromPTXFile( ptx_path, "miss" ) );
	m_context["bg_color"]->setFloat( 0.7f, 0.7f, 0.7f );

	m_ratio = static_cast<float>(m_width) / static_cast<float>(m_height);

	sutil::calculateCameraVariables(
		  m_eye, m_lookAt, m_up, m_hfov, m_ratio,
		  m_U, m_V, m_W );

	m_context["radiance_ray_type"	]->setUint( 0u );
	m_context["shadow_ray_type"		]->setUint( 1u );
	m_context[ "scene_epsilon"      ]->setFloat( 0.01 );
	m_context["jitter_factor"		]->setFloat( 1.0f);
	m_context["max_depth"			]->setInt( 4 );
	m_context["frame"				]->setInt( m_frameAccum );
	m_context["eye"					]->setFloat( m_eye );
	m_context["U"					]->setFloat( m_U );
	m_context["V"					]->setFloat( m_V );
	m_context["W"					]->setFloat( m_W );

	m_context->setAttribute(RT_CONTEXT_ATTRIBUTE_GPU_PAGING_FORCED_OFF, 1);

}
//
//=======================================================================================
//
void cOptixParticlesRenderer::createMaterial(	)
{
	Program chp = m_context->createProgramFromPTXFile ( "shaders/shaders.ptx", "closest_hit_radiance_AO_phong" );
	//Program ahs = m_context->createProgramFromPTXFile ( "shaders/shaders.ptx", "any_hit_shadow" );
	Program aho = m_context->createProgramFromPTXFile ( "shaders/shaders.ptx", "any_hit_occlusion_AO" );
	m_material = m_context->createMaterial();
	m_material->setClosestHitProgram(0, chp);
	m_material->setAnyHitProgram(1, aho);


	// Setting Lights
    //const float max_dim = fmaxf(m_aabb.extent(0), m_aabb.extent(1)); // max of x, y components
	float  max_dim = 100.0f;
    /*
    BasicLight lights[] = {
    	{ make_float3( 0.0f,  0.0f,  0.0f ),  make_float3( 0.7f, 0.7f, 0.7f ), 0, 0 },
        { make_float3(  1.0f,  1.0f ,  0.0f ), make_float3( 0.7f, 0.7f, 0.7f ), 0, 0 },
        { make_float3(  0.0f, -1.0f , -1.0f ), make_float3( 0.7f, 0.7f, 0.7f ), 0, 0 }
    };
    lights[0].pos *= max_dim * 10.0f;
    lights[1].pos *= max_dim * 10.0f;
    lights[2].pos *= max_dim * 10.0f;
*/

    BasicLight lights[] = {
    	{ m_aabb.m_min,  make_float3( 1.0f, 1.0f, 1.0f ), 0, 0 },
        { m_aabb.m_max, make_float3( 1.0f, 1.0f, 1.0f ), 1, 0 },
        { make_float3(  0.0f, 0.0f , 0.0f ), make_float3( 1.0f, 1.0f, 1.0f ), 0, 0 }
    };


    Buffer light_buffer = m_context->createBuffer( RT_BUFFER_INPUT );
    light_buffer->setFormat( RT_FORMAT_USER );
    light_buffer->setElementSize( sizeof( BasicLight ) );
    light_buffer->setSize( sizeof(lights)/sizeof(lights[0]) );
    memcpy(light_buffer->map(), lights, sizeof(lights));
    light_buffer->unmap();

    // phong illumination shader
    m_context["lights"		]->set( light_buffer );
    m_context["Ka"			]->setFloat(1.0f, 1.0f, 1.0f);
    m_context["Kd"			]->setFloat(1.0f, 1.0f, 1.0f);
    m_context["Ks"			]->setFloat(1.0f, 1.0f, 1.0f);
    m_context["Kr"			]->setFloat(0.0f, 0.0f, 0.0f);
    m_context["phong_exp"	]->setFloat( 200.0f );
    m_context["ambient_light_color"]->setFloat(1.0f,1.0f,1.0f);

    // AO shader
    m_context["occlusion_distance"]->setFloat(100.0f);
    m_context["sqrt_occlusion_samples"]->setInt(4);

//    Program transparent_ch = m_context->createProgramFromPTXFile( "shaders/transparent.ptx", "closest_hit_radiance" );
//    Program transparent_ah = m_context->createProgramFromPTXFile( "shaders/transparent.ptx", "any_hit_shadow" );
}
//
//=======================================================================================
//
void cOptixParticlesRenderer::createGeometry (std::vector<float> *pos, float *min, float *max)
{
	unsigned int 	i = 0, j, k, numParticles;
	unsigned int	posIdx = 0;
	unsigned int 	ncolors = 128;
	double 			color[3*ncolors];
	cColorTable 	colorTab ("orange");

	colorTab.Sample (ncolors, color);

	numParticles 	= pos->size()/4;
	m_numGroups		= numParticles / (NUM_PARTICLES_PER_GROUP);
	m_sphere = new Geometry[m_numGroups+1];
	//m_numGroups = 1;
	std::cout << "Particles: " << numParticles << " Groups: " << m_numGroups << std::endl;

	for (k=0; k<m_numGroups; k++ )
	{
		Buffer colorBuffer 	= m_context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT4, NUM_PARTICLES_PER_GROUP );
		Buffer posBuffer	= m_context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT4, NUM_PARTICLES_PER_GROUP );
		float4*	positions 	= reinterpret_cast<float4*>(posBuffer->map());
		float4*	colors		= reinterpret_cast<float4*>(colorBuffer->map());

		for( i = 0; i < NUM_PARTICLES_PER_GROUP; i++ )
		{
			positions->x	= pos->operator [](posIdx  );
			positions->y 	= pos->operator [](posIdx+1);
			positions->z 	= pos->operator [](posIdx+2);
			positions->w 	= 1.0f;

			float	energy 	= pos->operator [](posIdx+3);
			int   colorIdx	= (int) (MapValueToNorm(energy, min[3], max[3], false)  * (ncolors-1));
			//int   colorIdx	= (int) (MapValueToNorm(energy, 0, 1, false)  * (ncolors-1));
			colors->x		= (float)color[colorIdx*3    ];
			colors->y		= (float)color[colorIdx*3 + 1];
			colors->z		= (float)color[colorIdx*3 + 2];
			colors->w		= 1.0;

			positions++;
			colors++;

			posIdx+=4;

		}
		posBuffer->unmap();
		colorBuffer->unmap ();

		m_sphere[k] = m_context->createGeometry();
		m_sphere[k]->setPrimitiveCount(NUM_PARTICLES_PER_GROUP);
		m_sphere[k]->setBoundingBoxProgram( m_context->createProgramFromPTXFile( "shaders/particles.ptx", "bounds" ) );
		m_sphere[k]->setIntersectionProgram( m_context->createProgramFromPTXFile( "shaders/particles.ptx", "robust_intersect" ) );
		//particles->setIntersectionProgram( m_context->createProgramFromPTXFile( "shaders/particles.ptx", "sphere_array_intersect" ) );
		m_sphere[k]["particle_buffer"]->setBuffer ( posBuffer );
		m_sphere[k]["color_buffer"]->setBuffer (colorBuffer);
		m_sphere[k]["radius"]->setFloat(4.0f);
	}

	// loading the remaining particles
	unsigned int remainingParticles = numParticles - posIdx/4;
	Buffer colorBuffer 	= m_context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT4, remainingParticles );
	Buffer posBuffer	= m_context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT4, remainingParticles );
	float4*	positions 	= reinterpret_cast<float4*>(posBuffer->map());
	float4*	colors		= reinterpret_cast<float4*>(colorBuffer->map());
	for (i=0; i< remainingParticles;i++ )
	{
		positions->x	= pos->operator [](posIdx  );
		positions->y 	= pos->operator [](posIdx+1);
		positions->z 	= pos->operator [](posIdx+2);
		positions->w 	= 15.0f;

		float	energy 	= pos->operator [](posIdx+3);
		//int   colorIdx	= (int) (MapValueToNorm(energy, min[3], max[3], false)  * ncolors);
		int   colorIdx	= (int) (MapValueToNorm(energy, -2.5, -2.0, false)  * (ncolors-1));
		colors->x		= (float)color[colorIdx*3    ];
		colors->y		= (float)color[colorIdx*3 + 1];
		colors->z		= (float)color[colorIdx*3 + 2];
		colors->w		= 1.0;

		positions++;
		colors++;

		posIdx+=4;
	}
	posBuffer->unmap();
	colorBuffer->unmap ();
	m_numGroups++; // adding a group for the remaining particles

	m_sphere[k] = m_context->createGeometry();
	m_sphere[k]->setPrimitiveCount(remainingParticles);
	m_sphere[k]->setBoundingBoxProgram( m_context->createProgramFromPTXFile( "shaders/particles.ptx", "bounds" ) );
	m_sphere[k]->setIntersectionProgram( m_context->createProgramFromPTXFile( "shaders/particles.ptx", "robust_intersect" ) );
	//particles->setIntersectionProgram( m_context->createProgramFromPTXFile( "shaders/particles.ptx", "sphere_array_intersect" ) );
	m_sphere[k]["particle_buffer"]->setBuffer ( posBuffer );
	m_sphere[k]["color_buffer"]->setBuffer (colorBuffer);


	// One side of the box
	//Min = {-0.246244, -0.241258, -2343.57 }
	//Max = {987.647, 987.656, 1059.79 }
	//Min = {-0.246244, -0.241258, -2343.57 }
	//Max = {987.647, 987.656, 1059.79 }
/*
	Aabb aabb;
	aabb.set(make_float3 (-0.246244, -0.241258, -2343.57), make_float3(987.647, 987.656, 1059.79));
	m_cube[0] = m_context->createGeometry();
	m_cube[0]->setPrimitiveCount( 1u );
	m_cube[0]->setBoundingBoxProgram( m_context->createProgramFromPTXFile( "shaders/parallelogram.ptx", "bounds" ) );
	m_cube[0]->setIntersectionProgram( m_context->createProgramFromPTXFile( "shaders/parallelogram.ptx", "intersect" ) );
	//float3 anchor = make_float3( m_aabb., 0.01f, m_aabb.m_max.z/2.0f);
	//float3 anchor = make_float3(m_aabb.center().x-200, 0.0f, m_aabb.center().z+200);
	//float3 anchor = make_float3( 0.0f, 0.0f, 0.0f);
	//float3 anchor = make_float3 (aabb.m_min.x, 0.0f, aabb.m_min.z);
	float3 anchor = make_float3 (aabb.m_min.x, 0.0f, aabb.m_min.z);
	float3 v1 = make_float3( fabs(aabb.m_min.x)+fabs(aabb.m_max.x), 0, 0);
	float3 v2 = make_float3( 0, 0.0f, fabs(aabb.m_min.z)+fabs(aabb.m_max.z));
	float3 normal = cross( v1, v2 );
	normal = normalize( normal );
	float d = dot( normal, anchor );
	v1 *= 1.0f/dot( v1, v1 );
	v2 *= 1.0f/dot( v2, v2 );
	float4 plane = make_float4( normal, d );
	m_cube[0]["plane"]->setFloat( plane );
	m_cube[0]["v1"]->setFloat( v1 );
	m_cube[0]["v2"]->setFloat( v2 );
	m_cube[0]["anchor"]->setFloat( anchor );
*/

}
//
//=======================================================================================
//
void cOptixParticlesRenderer::createInstance ()
{
	unsigned int i;

	// Create geometry group
	Group group = m_context->createGroup();
	group->setChildCount( m_numGroups );
	group->setAcceleration( m_context->createAcceleration("Trbvh") );

	for (i=0; i<m_numGroups; ++i)
	// for (i=0; i<NUM_GROUPS; ++i)  //use this for sintetic particles
	{

		GeometryInstance gi = m_context->createGeometryInstance();
		// Create geometry instance
		gi = m_context->createGeometryInstance();
		gi->setGeometry( m_sphere[i] );
		gi->setMaterialCount( 1 );
		gi->setMaterial( 0, m_material );

		GeometryGroup geometrygroup = m_context->createGeometryGroup();
		geometrygroup->setAcceleration( m_context->createAcceleration("Trbvh") );
		geometrygroup->setChildCount(1);
		geometrygroup->setChild( 0, gi );
		group->setChild( i, geometrygroup );
	}

	m_context["top_object"]->set( group );
    m_context["top_shadower"]->set( group );

}
//
//=======================================================================================
//
void cOptixParticlesRenderer::resetAccumulation ()
{
	m_frameAccum = 0;
	m_context[ "frame"                  ]->setUint( 0 );
	//m_context[ "sqrt_occlusion_samples"	]->setInt(1);
	//m_context[ "occlusion_distance"		]->setFloat(50.0f);
}
//
//=======================================================================================
//
void cOptixParticlesRenderer::setBufferIds( const std::vector<Buffer>& buffers, Buffer top_level_buffer )
{
	top_level_buffer->setSize( buffers.size() );
	int* data = reinterpret_cast<int*>( top_level_buffer->map() );
	for( unsigned i = 0; i < buffers.size(); ++i )
		data[i] = buffers[i]->getId();
	top_level_buffer->unmap();
}
//
//=======================================================================================
//
#ifdef POST_PROCESSING
void cOptixParticlesRenderer::setupPostprocessing( )
{
	// create stages only once: they will be reused in several command lists without being re-created
	m_tonemapStage = m_context->createBuiltinPostProcessingStage("TonemapperSimple");
	m_denoiserStage = m_context->createBuiltinPostProcessingStage("DLDenoiser");

	    // if an alternative trained file is available:
//	    if (trainingDataBuffer)
//	    {
//	    	Variable trainingBuff = denoiserStage->declareVariable("training_data_buffer");
//	        trainingBuff->set(trainingDataBuffer);
//	    }

	m_tonemapStage->declareVariable("input_buffer")->set( m_context["output_buffer"]->getBuffer());
	m_tonemapStage->declareVariable("output_buffer")->set( m_context["tonemapped_buffer"]->getBuffer());
	m_tonemapStage->declareVariable("exposure")->setFloat(0.25f);
	m_tonemapStage->declareVariable("gamma")->setFloat(2.2f);

	//m_denoiserStage->declareVariable("input_buffer")->set(m_context["tonemapped_buffer"]->getBuffer());
	m_denoiserStage->declareVariable("input_buffer")->set(m_context["output_buffer"]->getBuffer());
	m_denoiserStage->declareVariable("output_buffer")->set(m_denoisedBuffer);
    m_denoiserStage->declareVariable("albedo_buffer");
    m_denoiserStage->declareVariable("normal_buffer");



	// Create two command lists with two postprocessing topologies we want:
	// One with the denoiser stage, one without. Note that both share the same
	// tonemap stage.

	m_clDenoiser = m_context->createCommandList();
	m_clDenoiser->appendLaunch(0, m_width, m_height);
	//m_clDenoiser->appendPostprocessingStage(m_tonemapStage, m_width, m_height);
	m_clDenoiser->appendPostprocessingStage(m_denoiserStage, m_width, m_height);
	m_clDenoiser->finalize();


}
#endif
//
//=======================================================================================
//
void cOptixParticlesRenderer::onKeyboardEvent ()
{
	if ( m_keyboardHandler->getState() == cKeyboardHandler::DOWN )
	{
		int key = m_keyboardHandler->getKey();

		switch (key)
		{
		case 'd':
			m_denoiserEnabled = !m_denoiserEnabled;
		}
	}

}
