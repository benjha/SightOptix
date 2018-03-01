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


// load dataset with a decimation factor.  Useful when data does not fit in GPU Memory

int loadAscii (const char* filename, std::vector<float> *positions, std::vector<float> *nrg, float *min, float *max, unsigned int decimation)
{
	int numParticles = 0;
	static int numComponents = 4;
	std::cout << "loading " << filename << std::endl;
	// first parse the file
	std::string 	point;
	std::ifstream 	file (filename, std::ios::binary);
	char 			*memblock;

	double x, y, z, energy, id;

	if (!file.is_open())
	{
		return false;
	}

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
	unsigned int  recordsRead = 0;
	 //float x = strtod(memblock, &end);

	while (ptr != end)
	{

		if (numParticles%decimation == 0)
		{
			if (end)
			{
				ptr = end;
			}

			x = strtof(ptr, &end);
			ptr = end;

			y = strtof(ptr, &end);
			ptr = end;

			z = strtof(ptr, &end);
			ptr = end;

			energy = strtof(ptr, &end);

			if (numParticles == 0)
			{
				min[0] = x;
				min[1] = y;
				min[2] = z;
				min[3] = energy;

				max[0] = x;
				max[1] = y;
				max[2] = z;
				max[3] = energy;
			}
			else
			{
				min[0] = Min<float> (x, min[0]);
				min[1] = Min<float> (y, min[1]);
				min[2] = Min<float>  (z, min[2]);

				max[0] = Max<float> (x, max[0]);
				max[1] = Max<float> (y, max[1]);
				max[2] = Max<float> (z, max[2]);

				min[3] = Min<float> (energy,min[3]);
				max[3] = Max<float> (energy,max[3]);
			}


			positions->push_back 	(x);
			positions->push_back 	(y);
			positions->push_back 	(z);
			positions->push_back 	(energy);

			if (positions->size()%(1000000*numComponents) == 0 && numParticles > 1) // print every million atoms read
			{
				recordsRead = (positions->size()/numComponents) / 1000000;

				std::cout << "Records read: " << recordsRead << "M of " << numParticles / 1000000 << "M" << std::endl; // divide by 3 because holds 3 variables.

			}
		}
		else // just go through the buffer
		{
			if (end)
				ptr = end;

			//x
			x = strtof(ptr, &end);
			ptr = end;

			//y
			y = strtof(ptr, &end);
			ptr = end;

			//z
			z = strtof(ptr, &end);
			ptr = end;

			//energy
			energy = strtof(ptr, &end);
			//ptr = end;
			//id
			//id = strtof(ptr, &end);

		}
		numParticles++;
	}
	delete [] memblock;

	std::cout << "Num atoms: " 			<< " " << positions->size()/numComponents << std::endl;
	//std::cout << "Num elements: " 			<< " " << vPosNRG.size()	<< std::endl;

	std::cout << "Min = {" << min[0] 	<< ", " << min[1] << ", " << min[2] << " } \n";
	std::cout << "Max = {" << max[0] 	<< ", " << max[1] << ", " << max[2] << " } \n";
	std::cout << "Energy min: " << min[3] << " Energy max: " << max[3] << std::endl;

	file.close();

	return true;

}

