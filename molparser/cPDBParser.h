/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 */
#ifndef CPDBREADER_H_
#define CPDBREADER_H_

#include <string>
#include <vector>
#include <memory>
#include "../molparser/readpdb.h"
#include "../molparser/molfile_plugin.h"
#include "../molparser/pdbplugin.h"


class cPDBParser
{
public:

	cPDBParser ( bool freeMemory=false);
	~cPDBParser	();

	int	parse (std::string filename);
	// gets number of steps
	size_t	timesteps() {return vTimestep.size();};
	// gets number of atoms
	size_t	numberOfAtoms() {return natoms;};
	// gets atom attributes
	molfile_atom_t *atomAttribs() {return atoms;};
	// gets nth timestep
	molfile_timestep_t timestep(size_t n) {return vTimestep[n];};

private:

	int natoms;
	int rc;
	int nsteps;
	int optflags;
	bool freeMem;
	pdbdata *PDBdata;
	molfile_atom_t *atoms;
	std::vector
	<molfile_timestep_t> vTimestep;
};

#endif /* CPDBPARSER_H_ */
