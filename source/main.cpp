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

// Renderer
#include "../header/cOptixParticlesRenderer.h"

int						front			= 0;	// buffer
int						back			= 1;
double 					theta 			= 0.0f;
bool 					running 		= true;
bool					flag			= true;
float3 					cam_eye 		= { 0.0f, 0.0f, 5.0f };
broadcast_server		*wsserver 		= 0;
cMouseHandler 			*mouseHandler 	= 0;
cKeyboardHandler 		*keyboardHandler= 0;
cMessageHandler 		*msgHandler 	= 0;
cOptixParticlesRenderer *renderer	= 0;
unsigned char			pixels[IMAGE_WIDTH*IMAGE_HEIGHT*3];

void display ()
{
	static bool flag = 1;
	renderer->display(pixels);

	if (wsserver->sendMoreFrames())
	{
		try
		{
			wsserver->sendFrame(pixels);
		}
		catch ( Exception& e )
		{
			std::cout << e.getErrorString().c_str() << std::endl;
			exit(1);
		}
	}

	if (flag)
	{
		std::cout << "Websocket: " << std::endl;
		flag=!flag;
	}
}
//
//=======================================================================================
//
void renderingLoop ()
{
	char c;
	while (running)
	{
		display (	);
	}
}
//
//=======================================================================================
//
void webSocketServer() {
	std::cout << "launching server at port " << SERVER_PORT << std::endl;
	std::cout << "Press CTRL+C to exit...\n" << std::endl;
	wsserver->run(SERVER_PORT);

	std::cerr << "Exiting websockets thread!" << std::endl;
}
//
//=======================================================================================
//
void init (int argc, char** argv)
{
	int			decimation = 1;
	std::string filename;
	std::vector<float> vPos;
	std::vector<float> vNrg;
	std::vector<float> data;
	// min/max Position and Energy values:
	float min[4] = {0.0,0.0,0.0, 0.0};
	float max[4] = {-100000.0,-100000.0,-100000.0, -100000.0};

	if ( argc == 3 )
	{
		filename = std::string (argv[1]);
		decimation = std::stoi (argv[2]);
	}
	else
	{
		std::cout << "\n Usage: \n";
		std::cout << "\t\t sight [file] decimationFactor \n\n";
		exit (1);
	}

// loader for files containing fields x,y,z,Pe
	if (!loadAscii(filename.data(), &vPos, &vNrg, min, max, decimation))
	{
		std::cout << filename << " file not found. " << std::endl;
		exit (0);
	}
	renderer = new cOptixParticlesRenderer ();
	renderer->init( IMAGE_WIDTH, IMAGE_HEIGHT, &vPos, min, max );

	mouseHandler 	= new cMouseHandler();
	keyboardHandler = new cKeyboardHandler();
	msgHandler 		= new cMessageHandler();
	wsserver 		= new broadcast_server();

}
//
//=======================================================================================
//
void setHandlers()
{
	wsserver->setMouseHandler		(mouseHandler);
	wsserver->setKeyboardHandler	(keyboardHandler);
	wsserver->setMessageHandler		(msgHandler);
	renderer->setMouseHandler 		(mouseHandler);
}
//
//=======================================================================================
//
void signalHandler (int signum)
{
	running = false;
	wsserver->stop_listening();
}
//
//=======================================================================================
//
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

	delete 	mouseHandler;
	delete 	keyboardHandler;
	delete 	msgHandler;
	delete 	wsserver;
	delete	renderer;

	return 0;
}
