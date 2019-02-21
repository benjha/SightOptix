/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 */

#define ASIO_STANDALONE

#include <thread>
#include <string.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <csignal>
#include <cmath>
#include <string>
#include <vector>
#include "../header/loaders.h"
// websockets headers
#include "../frameserver/header/cBroadcastServer.h"
#include "../frameserver/header/cMouseEventHandler.h"
#include "../frameserver/header/cKeyboardHandler.h"
#include "../frameserver/header/cMessageHandler.h"
#include "../parsers/cPDBParser.h"
#include "../header/cParticles.h"
// Renderer
#include "../header/cOptixRenderer.h"

int front=0;	// buffer
int back=1;
double theta=0.0f;
bool running=true;
bool flag= true;
float3 cam_eye= { 0.0f, 0.0f, 5.0f };
broadcast_server *wsserver=0;
cMouseHandler *mouseHandler= 0;
cKeyboardHandler *keyboardHandler=0;
cMessageHandler *msgHandler= 0;
cOptixRenderer *renderer=0;
cParticles *particles=0;
#ifdef NVPIPE_ENCODING
unsigned char pixels[IMAGE_WIDTH*IMAGE_HEIGHT*4];
#else
unsigned char pixels[IMAGE_WIDTH*IMAGE_HEIGHT*3];
#endif

void display ()
{
	static bool flag = 1;
	renderer->display(pixels);

	if (wsserver->sendMoreFrames())
	{
		try
		{
#ifdef REMOTE_GPU_ENCODING
			// Use the next line only when using GPU encoding
			wsserver->sendFrame(renderer->getGPUFrameBufferPtr());
#endif
#if defined(REMOTE) || defined(NO_COMPRESSION)
			renderer->getPixels(pixels);
			wsserver->sendFrame(pixels);
#endif
		}
		catch ( Exception& e )
		{
			std::cout << e.getErrorString().c_str() << std::endl;
			exit(1);
		}
	}
	if (wsserver->saveFrame())
	{
		renderer->getPixels(pixels);
		wsserver->save(pixels);
	}
#ifdef STATS
	wsserver->printStats ();
#endif

	if (flag)
	{
		std::cout << "Websocket: " << std::endl;
		flag=!flag;
	}
}

void renderingLoop ()
{
	char c;
	while (running)
	{
		display ();
	}
}

void webSocketServer() {
	std::cout << "launching server at port " << SERVER_PORT << std::endl;
	std::cout << "Press CTRL+C to exit...\n" << std::endl;
	wsserver->run(SERVER_PORT);

	std::cerr << "Exiting websockets thread!" << std::endl;
}

void init (int argc, char** argv)
{
	int			decimation = 1;
	std::string filename;
	std::vector<float> vPos;
	std::vector<float> vNrg;
	std::vector<float> data;
	// min/max Position and Energy values:
	float min[4] = {0.0,0.0,0.0, 0.0};
	float max[4] = {0.0,0.0,0.0, 0.0};

	if ( argc == 2 )
	{
		filename = std::string (argv[1]);
		//decimation = std::stoi (argv[2]);
	}
	else
	{
		std::cout << "\n Usage: \n";
		//std::cout << "\t\t sight [file] decimationFactor \n\n";
		std::cout << "\t\t sight [file] \n\n";
		exit (1);
	}

	std::string ext = getFileExtension(filename);
	std::cout << "Reading " << filename << "... " << std::endl;

	int nsteps					= 0;
	int optflags				= 0;
	size_t i;

	if (ext.compare("pdb") == 0)
	{
		cPDBParser pdb;
		pdb.parse(filename.data());
		if (pdb.timesteps()>0)
		{
			nsteps = pdb.timesteps();
			particles = new cParticles[nsteps];
			//		molfile_timestep_t *p = pdb.vTimestep[0];
			std::cout << "loadFromPDB" << std::endl;
			for (i=0;i<nsteps;i++)
				particles[i].loadFromPDB(pdb.numberOfAtoms(), pdb.atomAttribs(), pdb.timestep(i));

			std::cout << "Particles: " << particles[0].numParticles() << std::endl;
			std::cout << "Min = { " << particles[0].posMin()[0] << ", " << particles[0].posMin()[1]
									<< ", " << particles[0].posMin()[2] << " }" << std::endl;
			std::cout << "Max = { " << particles[0].posMax()[0] << ", " << particles[0].posMax()[1]
									<< ", " << particles[0].posMax()[2] << " }" << std::endl;
		}
	}
	// loader for files containing fields x,y,z,Pe
	else if (ext.compare("txt")==0)
	{
		nsteps = 1;
		particles = new cParticles[nsteps];
		if(!parseAscii(filename.data(), particles, decimation))
		{
			std::cout << filename << " file not found. " << std::endl;
			exit (1);
		}
		//particles->genColors ();
		std::cout << "Particles: " << particles[0].numParticles() << std::endl;
		std::cout << "Min = { " << particles[0].posMin()[0] << ", " << particles[0].posMin()[1]
								<< ", " << particles[0].posMin()[2] << " }" << std::endl;
		std::cout << "Max = { " << particles[0].posMax()[0] << ", " << particles[0].posMax()[1]
								<< ", " << particles[0].posMax()[2] << " }" << std::endl;
	}
	else
	{
		std::cout << "Extension not supported.\n";
		exit(1);
	}

	renderer = new cOptixRenderer (true);
	renderer->init(IMAGE_WIDTH, IMAGE_HEIGHT, particles[0].posMin(), particles[0].posMax(), nsteps);
	size_t step, group;
	//std::cout << "Particles per group: " << std::endl;
	for (step = 0; step<nsteps; step++)
	{
		for (group = 0; group<particles[step].groups();group++)
		{
			renderer->loadSpheres( particles[step].numParticles(group), step,
                                   particles[step].pos(group),
                                   particles[step].color(group), particles[step].radius(group) );
		}
		// TODO:
		//renderer->addTimeStep ();
	}
	renderer->genOptixRoot();
	renderer->launch();

	mouseHandler 	= new cMouseHandler();
	keyboardHandler = new cKeyboardHandler();
	msgHandler 		= new cMessageHandler();
	wsserver 		= new broadcast_server();

}

void setHandlers()
{
	wsserver->setMouseHandler		(mouseHandler);
	wsserver->setKeyboardHandler	(keyboardHandler);
	wsserver->setMessageHandler		(msgHandler);
	renderer->setMouseHandler 		(mouseHandler);
	renderer->setKeyboardHandler	(keyboardHandler);
}

void signalHandler (int signum)
{
	running = false;
	wsserver->stop_listening();
}

void freeResources ()
{
	if (mouseHandler)
		delete 	mouseHandler;
	if (keyboardHandler)
		delete 	keyboardHandler;
	if (msgHandler)
		delete 	msgHandler;
	if (wsserver)
		delete 	wsserver;
	if (renderer)
		delete	renderer;
	if (particles)
		delete [] particles;
}

int main(int argc, char** argv)
{

	srand (time(NULL));
	signal(SIGINT, signalHandler);

	init (argc, argv);

	setHandlers				(	);
	std::thread wsserverThread	( webSocketServer );

	renderingLoop				(	);

	std::cout << "Exiting...\n";

	wsserverThread.join			(	);

	freeResources ();

	return 0;
}
