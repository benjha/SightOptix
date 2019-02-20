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
#include "../molparser/cPDBParser.h"
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
float *color;

void setColorBuffer			( void *atoms, unsigned int numAtoms )
{
	unsigned int i = 0;
	molfile_atom_t *data = (molfile_atom_t *)atoms;
	color		= new float	[numAtoms*4];
	//m_radius 		= new float [numAtoms];



	for (i=0; i< numAtoms; i++)
	{
		//m_radius[i] = get_pte_vdw_radius(data[i].atomicnumber);
		//std::cout << data[i].name << "\t";
		if (data[i].atomicnumber == 6) // carbon - emerald
		{
			color[i*4    ] = 0.0f;
			color[i*4 + 1] = 153.0/255.0f;
			color[i*4 + 2] = 117.0/255.0f;
			color[i*4 + 3] = 1.0f;

		}
		else if (data[i].atomicnumber == 8 ) // oxygen - red
		{
			color[i*4    ] = 1.0f;
			color[i*4 + 1] = 0.0f;
			color[i*4 + 2] = 0.0f;
			color[i*4 + 3] = 1.0f;
		}
		else if (data[i].atomicnumber == 1 ) // hydrogen - white
		{
			color[i*4    ] = 1.0f;
			color[i*4 + 1] = 1.0f;
			color[i*4 + 2] = 1.0f;
			color[i*4 + 3] = 1.0f;
		}
		else if (data[i].atomicnumber == 7 ) // Nytrogen - light blue
		{
			color[i*4    ] = 143.0/255.0f;
			color[i*4 + 1] = 143.0/255.0f;
			color[i*4 + 2] = 1.0;
			color[i*4 + 3] = 1.0f;
		}
		else if (data[i].atomicnumber == 16 ) // Sulfur - yellow
		{
			color[i*4    ] = 1.0f;
			color[i*4 + 1] = 200.0/255.0f;
			color[i*4 + 2] = 50.0/255.0f;
			color[i*4 + 3] = 1.0f;

		}
		else if (data[i].atomicnumber == 15 || data[i].atomicnumber == 26 ) //Phosphorus/Iron - orange
		{
			color[i*4    ] = 255.0/255.0f;
			color[i*4 + 1] = 165.0/255.0f;
			color[i*4 + 2] = 0.0f;
			color[i*4 + 3] = 1.0f;

		}
		else if (data[i].atomicnumber == 17) // Chlorine - green
		{
			color[i*4    ] = 0.0;
			color[i*4 + 1] = 1.0;
			color[i*4 + 2] = 0.0;
			color[i*4 + 3] = 1.0f;
		}
		else if (data[i].atomicnumber == 35 || data[i].atomicnumber == 30) //Bromin/Zin - brown
		{
			color[i*4    ] = 165.0/255.0f;
			color[i*4 + 1] = 42.0/255.0f;
			color[i*4 + 2] = 42.0/255.0f;
			color[i*4 + 3] = 1.0f;
		}
		else if (data[i].atomicnumber == 11 ) // Sodium - blue
		{
			color[i*4    ] = 0.0f;
			color[i*4 + 1] = 0.0f;
			color[i*4 + 2] = 1.0f;
			color[i*4 + 3] = 1.0f;
		}
		else if (data[i].atomicnumber == 12 || data[i].atomicnumber == 20 ) // Magnesium Calcium - dark green
		{
			color[i*4    ] = 42.0/255.0f;
			color[i*4 + 1] = 128.0/255.0f;
			color[i*4 + 2] = 42.0/255.0f;
			color[i*4 + 3] = 1.0f;
		}
		else // other/unknown Pink
		{
			color[i*4    ] = 1.0f;
			color[i*4 + 1] = 20.0/255.0f;
			color[i*4 + 2] = 147.0/255.0f;
			color[i*4 + 3] = 1.0f;
		}
	}
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

	// storage when file type is PDB
	molfile_atom_t 		*atoms;
	molfile_timestep_t 	timestep;
	pdbdata				*PDBdata;
	int					natoms;
	int rc;
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

			for (i=0;i<nsteps;i++)
			{
				//std::cout << i << std::endl;
				particles[i].loadFromPDB(pdb.numberOfAtoms(), pdb.atomAttribs(), pdb.timestep(i));
			}
			std::cout << "PDB. natoms " << pdb.numberOfAtoms() << std::endl;
			std::cout << "Particles. natoms " << particles[0].numParticles() << std::endl;

			std::cout << "Min = { " << particles[0].minPos()[0] << ", " << particles[0].minPos()[1]
									<< ", " << particles[0].minPos()[2] << " }" << std::endl;
			std::cout << "Max = { " << particles[0].maxPos()[0] << ", " << particles[0].maxPos()[1]
									<< ", " << particles[0].maxPos()[2] << " }" << std::endl;
		}



		//std::cout << "PDB \n";
		//for (i=0;i<25;i++)
		//{
		//	std::cout << pdb.timestep(0).coords[i*4] << " " << pdb.timestep(0).coords[i*4+1] << " " << pdb.timestep(0).coords[i*4+2] << std::endl;
		//}
		//std::cout << "Particle " << particles[0].groups () << std::endl;
//		for (i=0;i<25;i++)
//		{
//			std::cout << particles[0].pos[0][i*4] << " " << particles[0].pos[0][i*4+1] << " "
//					  << particles[0].pos[0][i*4+2] << " " << particles[0].pos[0][i*4+3] << std::endl;
//		}
		//pdb.~cPDBParser();
/*
		PDBdata = (pdbdata *)open_pdb_read(filename.data(), 0, &natoms);
		if (!PDBdata)
		{
			std::cout << "PDdata Error " << std::endl;
			exit(0);
		}
		std::cout << natoms << " atoms read.\n";
		atoms = new molfile_atom_t[natoms];
		rc = read_pdb_structure (PDBdata, &optflags, atoms);
		if (rc)
		{
			std::cout << " rc Error " << std::endl;
			delete [] atoms;
			exit (0);
		}
		setColorBuffer (atoms, natoms);
		std::cout << "Reading coordinates... " << std::endl;
		timestep.velocities = 0;
	    timestep.coords = new float [4*natoms];
		rc = read_next_timestep(PDBdata,natoms, &timestep);

		//for (int i=0;i<natoms;i++)
		//{
		//	std::cout << " Atom " << i << " " << atoms[i].resname << std::endl;
		//}

		if (rc)
		{
			std::cerr << " rc Error " << std::endl;
			delete [] atoms;
			delete [] timestep.coords;
			exit(1);
		}

		close_pdb_read(PDBdata);

		std::cout << "Done!" << std::endl;

		std::cout << "Finding the bounding box of the system... " << std::endl;
		find_minmax_all (timestep.coords, natoms, min, max);
		std::cout << "Done! " << std::endl;

		for (int i=0;i<natoms;i++)
		{
			//std::cout <<  timestep.coords[i*4] << " " << timestep.coords[i*4+1] << " " << timestep.coords[i*4+2] << " " << timestep.coords[i*4+3] <<  std::endl;
			vPos.push_back(timestep.coords[i*4]);
			vPos.push_back(timestep.coords[i*4+1]);
			vPos.push_back(timestep.coords[i*4+2]);
			vPos.push_back(i%4);
		}
		*/
	}
	// loader for files containing fields x,y,z,Pe
	else if (!loadAscii(filename.data(), &vPos, &vNrg, min, max, decimation))
	{
		std::cout << filename << " file not found. " << std::endl;
		exit (0);
	}
//	std::cout << "Min = {" << min[0] 	<< ", " << min[1] << ", " << min[2] << " } \n";
//	std::cout << "Max = {" << max[0] 	<< ", " << max[1] << ", " << max[2] << " } \n";

	renderer = new cOptixRenderer (true);
	renderer->init(IMAGE_WIDTH, IMAGE_HEIGHT, particles[0].minPos(), particles[0].maxPos());
	size_t step, group;
	for (step = 0; step<nsteps; step++)
		for (group = 0; group<particles[step].groups();group++)
		{
			renderer->loadSpheres( particles[step].numParticles(), group,
                                   particles[step].pos(group),
                                   particles[step].color(group), particles[step].radii(group) );
		}
	return;
	/*
	std::cout << vPos.size() << " " << vPos.size()/4 << std::endl;

	min[3] = 0;
	max[3] = 3;
	renderer = new cOptixRenderer (true);
	renderer->init( IMAGE_WIDTH, IMAGE_HEIGHT, &vPos, min, max );
*/
	mouseHandler 	= new cMouseHandler();
	keyboardHandler = new cKeyboardHandler();
	msgHandler 		= new cMessageHandler();
	wsserver 		= new broadcast_server();

	if (ext.compare("pdb") == 0)
	{
		delete [] atoms;
		delete [] timestep.coords;
	}


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
	renderer->setKeyboardHandler	(keyboardHandler);
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
/*
	setHandlers				(	);
	std::thread wsserverThread	( webSocketServer );

	renderingLoop				(	);

	std::cout << "Exiting...\n";

	wsserverThread.join			(	);
*/
	delete 	mouseHandler;
	delete 	keyboardHandler;
	delete 	msgHandler;
	delete 	wsserver;
	delete	renderer;

	return 0;
}
