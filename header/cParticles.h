
#ifndef __CPARTICLES_H__
#define __CPARTICLES_H__

#include <vector>
#include "globals.h"
#include "../parsers/cPDBParser.h"

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
	// generate colors from -energy- values and color table
	void genColors ();
	// initializes groups given g
	void initNumGroups (size_t g) {m_nGroups=g;};
	// gets the total number of particles of whole system
	size_t numParticles () { return m_nParticles; };
	// increments numParticles counter
	void incNumParticles () { m_nParticles++; };
	// gets num. of particles of the nth group
	size_t numParticles(size_t n) {return m_pos[n].size()/4;};
	// gets the number of groups used to allocate the particles
	unsigned int groups () { return m_nGroups; };
	// gets model's bounding box
	boundingBox bBox () { return m_bb; };
	// gets pos vector
	std::vector<float> *pos () {return m_pos;};
	// gets pos array at nth group
	float *pos(size_t n) {return m_pos[n].data();};
	// gets min pos value
	float *posMin () { return m_posMin;};
	// gets max pos value
	float *posMax () { return m_posMax;};
	// gets energy vector
	std::vector<float> *nrg () {return m_nrg;};
	// gets min max energy values vector
	float *nrgMinMax () {return m_nrgMinMax;};
	// gets color vector
	std::vector<float> *color() {return m_color;};
	// gets color array at nth group
	float *color(unsigned int n) {return m_color[n].data();};
	// gets radius vector
	std::vector<float> *radius(){return m_radius;};
	// gets radius array at nth group
	float *radius(unsigned int n) {return m_radius[n].data();};
	// initialize min/max position and energy values
	void initMinMax (float *p, float nrg);
	// gets min/max values given position and energy values
	void findMinMax (float *p, float nrg);

private:
	//initializes particle colors by atomic number
	void initColor (size_t g, int atomicNumber);
	//initializes particle colors by atom name
	void initColor (size_t g, char name);
	// generates color given the atomic number
	// or the name of the particle
	void colorBy (float *color, int number=0, char name=' ');
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
	// Energy values retrieved from ascii files
	std::vector<float> m_nrg[NUM_GROUPS];
	// energy min-max values
	float m_nrgMinMax[2];
	//--- add additional variables on demand ---

	// bounding box of the system
	boundingBox			m_bb;
};

#endif /* __CPARTICLES_H__ */
