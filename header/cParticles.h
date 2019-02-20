
#ifndef __CPARTICLES_H__
#define __CPARTICLES_H__

#include <vector>
#include "globals.h"
#include "../molparser/cPDBParser.h"

typedef struct
{
	float coord0[4]; // x1, y1, z1, w1
	float coord1[4];
	float coord2[4];
	float coord3[4];
	float coord4[4];
	float coord5[4];
	float coord6[4];
	float coord7[4];
}boundingBox;

// A timestep of particles
class cParticles
{
public:
	enum
	{
		PDB = 0,
		ASCII
	};
	cParticles ();
	~cParticles ();
	// initializes particles from PDB dataset
	void loadFromPDB (size_t natoms, molfile_atom_t *atom,  molfile_timestep_t timestep);
	// gets number of particles
	size_t numParticles () { return m_nParticles; };
	// gets the number of groups used to allocate the particles
	unsigned int groups () { return m_nGroups; };
	// gets model's bounding box
	boundingBox bBox () { return m_bb; };
	// gets min pos value
	float *minPos () { return m_posMin;};
	// gets max pos value
	float *maxPos () { return m_posMax;};
	// gets pos array at nth group
	float *pos(unsigned int n) {return m_pos[n].data();};
	// gets color array at nth group
	float *color(unsigned int n) {return m_color[n].data();};
	// gets radius array at nth group
	float *radii(unsigned int n) {return m_radius[n].data();};

private:
	//initializes particle colors
	void initColor (size_t g, int atomicNumber);
	// get color given the atomic number of the particle
	void colorByAtomicNumber (int number, float *color);
	// initialize model's bounding box
	void initBB ( );
	// initialize min/max position values given p
	void initMinMaxPos (float *p);
	// gets position min/max values
	void findMinMaxPos (float *p);

	// total number of groups used to group particles
	unsigned int m_nGroups;
	size_t m_nParticles;
	// position per particle, stores x,y,z,w  given w = 1.0
	std::vector<float> m_pos[NUM_GROUPS];
	// position max value
	float m_posMax[3];
	// position min value
	float m_posMin[3];
	// color per particle, stores r,g,b,a
	std::vector<float> m_color[NUM_GROUPS];
	// radius per particle
	std::vector<float> m_radius[NUM_GROUPS];
	// particle's Id or sequential number coming from the dataset
	std::vector<size_t> m_id[NUM_GROUPS];
	// group id for a given particle, particles from the same group
	// e.g. resiudals, micro-clusters, sub-molecules
	std::vector<size_t> m_resid[NUM_GROUPS];
	//--- add additional variables on demand ---
	// bounding box of the system
	boundingBox			m_bb;
};

#endif /* __CPARTICLES_H__ */
