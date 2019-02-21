#include <iostream>
#include <cstring>
#include "cPDBParser.h"

cPDBParser::cPDBParser(bool freeMemory)
			: natoms(0), rc(0), nsteps(0), optflags(0), freeMem (freeMemory), PDBdata(0), atoms(0)
{

}

cPDBParser::~cPDBParser ()
{
	//std::cout << "~cPDBParser()\n";
	if (vTimestep.size()>0)
		for (std::vector<molfile_timestep_t>::iterator i = vTimestep.begin(); i != vTimestep.end(); ++i)
			if (i->coords)
			{
				delete[] i->coords;
				i->coords = 0;
			}

	if (!freeMem)
		close_pdb_read(PDBdata);

}

int cPDBParser::parse (std::string filename)
{
	PDBdata = (pdbdata *)open_pdb_read(filename.data(), 0, &natoms);
	if (!PDBdata)
	{
		std::cout << "Error reading PDB file.\n";
		return 0;
	}
	std::cout << natoms << " atoms read.\n";

	atoms = new molfile_atom_t[natoms];
	rc = read_pdb_structure (PDBdata, &optflags, atoms);
	if (rc)
	{
		std::cout << "Error reading structure\n";
		delete [] atoms;
		return 0;
	}
	std::cout << "Reading coordinates...\n";

	molfile_timestep_t 	timestep;
	timestep.velocities = 0;
	timestep.coords = new float [4*natoms];
	while (!(rc=read_next_timestep(PDBdata,natoms, &timestep)))
	{
		molfile_timestep_t 	ts;
		ts.velocities = 0;
		ts.coords = new float[4*natoms];
		std::memcpy((void*)ts.coords, (void*)timestep.coords, 4*natoms*sizeof(float) );
		vTimestep.push_back(ts);
	}
	delete [] timestep.coords;

	std::cout << "Number of timesteps: " << vTimestep.size() << std::endl;

	std::cout << "Done!.\n";

	if (freeMem)
	{
		close_pdb_read(PDBdata);
	}

	return 1;
}
