/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 */

#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <vector>
#include <stdlib.h>
#include <assert.h>
#include "../header/loaders.h"
#include "../header/cParticles.h"



std::string getFileExtension(std::string file)
{
    std::size_t found = file.find_last_of(".");
    return file.substr(found+1);
}


// load dataset with a decimation factor.  Useful when data does not fit in GPU Memory

int parseAscii (const char* filename, cParticles *p, unsigned int decimation)
{
	size_t j = 0;
	float cPos[3]; // current position
	size_t g = 0;
	std::cout << "loading " << filename << std::endl;

	// first parse the file
	std::ifstream 	file (filename, std::ios::binary);
	char 			*memblock;

	double x, y, z, energy, id;

	if (!file.is_open())
		return false;

	// get size of file
	file.seekg(0, file.end);
	std::size_t size=file.tellg();
	file.seekg(0);

	std::cout << "file size " << size << " bytes\nReading started\n";

	memblock	= new char [size];
	file.seekg	(0, std::ios::beg);
	file.read 	(memblock, size);
	file.close ( );

	std::cout << "Reading Done... Parsing starts... \n";

	char* ptr = memblock;
	char* end = 0;

	while (ptr != end)
	{
		if (p->numParticles()%decimation == 0)
		{
			if (end)
				ptr = end;

			cPos[0]=strtof(ptr, &end); // x
			ptr=end;
			cPos[1]=strtof(ptr, &end); // y
			ptr=end;
			cPos[2]=strtof(ptr, &end); // z
			ptr=end;
			energy=strtof(ptr, &end);

			if (p->numParticles() == 0)
				p->initMinMax (cPos, energy);
			else
				p->findMinMax(cPos, energy);

			p->pos()[g].push_back(cPos[0]); // x
			p->pos()[g].push_back(cPos[1]); // y
			p->pos()[g].push_back(cPos[2]); // z
			p->pos()[g].push_back(1.0f); // w
			p->radius()[g].push_back(1.44f);
			p->nrg()[g].push_back(energy);
			// calculate color later, but allocate space now
			p->color()[g].push_back(1.0f); // r
			p->color()[g].push_back(1.0f); // g
			p->color()[g].push_back(1.0f); // b
			p->color()[g].push_back(1.0f); // a

			if (j > PARTICLES_PER_GROUP)
			{
				g++;
				j=0;
			}
			p->incNumParticles();

			if (p->numParticles()%1000000 == 0) // print every millionth atoms read
			{
				std::cout << "Records read: " << p->numParticles() / 1000000 << "M" << std::endl;
			}
			j++;
		}
		else // just go through the buffer
		{
			if (end)
				ptr = end;

			cPos[0]=strtof(ptr, &end); // x
			ptr=end;
			cPos[1]=strtof(ptr, &end); // y
			ptr=end;
			cPos[2]=strtof(ptr, &end); // z
			ptr=end;
			energy=strtof(ptr, &end);
		}
	}
	p->initNumGroups (g+1);
	delete [] memblock;

	file.close();

	return true;

}
