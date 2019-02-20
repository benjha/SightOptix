#include <iostream>
#include "../header/cParticles.h"

cParticles::cParticles () : m_nParticles(0), m_nGroups(0)
{
}

cParticles::~cParticles ()
{

}

void cParticles::loadFromPDB(size_t natoms, molfile_atom_t *atom, molfile_timestep_t timestep)
{
	size_t i=0, g=0;
	float cPos[3];

	cPos[0] = timestep.coords[i*4];
	cPos[1] = timestep.coords[i*4+1];
	cPos[2] = timestep.coords[i*4+2];
	initMinMaxPos (cPos);
	for (i=1;i<natoms;i++)
	{
		cPos[0] = timestep.coords[i*4];
		cPos[1] = timestep.coords[i*4+1];
		cPos[2] = timestep.coords[i*4+2];
		//std::cout << i << " ";
		m_pos[g].push_back(cPos[0]); // x
		m_pos[g].push_back(cPos[1]); // y
		m_pos[g].push_back(cPos[2]); // z
		m_pos[g].push_back(1.0f); // w
		m_id[g].push_back(i);
		m_resid[g].push_back(atom[i].resid);
		m_radius[g].push_back(atom[i].radius);
		initColor (g, atom[i].atomicnumber);
		findMinMaxPos(cPos);
		if (i > PARTICLES_PER_GROUP)
			g++;
		m_nParticles++;
	}
	m_nGroups = g+1;
}

void cParticles::initColor (size_t g, int atomicNumber)
{
	float rgba[4];
	colorByAtomicNumber(atomicNumber, rgba);
	m_color[g].push_back(rgba[0]); // red
	m_color[g].push_back(rgba[1]); // green
	m_color[g].push_back(rgba[2]); // blue
	m_color[g].push_back(1.0f); // red
}

void cParticles::initMinMaxPos (float *p)
{
	m_posMin[0] = this->m_posMax[0] = p[0];
	m_posMin[1] = this->m_posMax[1] = p[1];
	m_posMin[2] = this->m_posMax[2] = p[2];
}

void cParticles::findMinMaxPos (float *p)
{
	m_posMin[0] = mIN<float> (p[0], m_posMin[0]);
	m_posMin[1] = mIN<float> (p[1], m_posMin[1]);
	m_posMin[2] = mIN<float> (p[2], m_posMin[2]);
	m_posMax[0] = mAX<float> (p[0], m_posMax[0]);
	m_posMax[1] = mAX<float> (p[1], m_posMax[1]);
	m_posMax[2] = mAX<float> (p[2], m_posMax[2]);
}


void cParticles::colorByAtomicNumber (int number, float *color)
{
	color[3] = 1.0f;
	if (number == 6) // carbon - emerald
	{
		color[0] = 0.0f;
		color[1] = 153.0/255.0f;
		color[2] = 117.0/255.0f;
	}
	else if (number == 8 ) // oxygen - red
	{
		color[0] = 1.0f;
		color[1] = 0.0f;
		color[2] = 0.0f;
	}
	else if (number == 1 ) // hydrogen - white
	{
		color[0] = 1.0f;
		color[1] = 1.0f;
		color[2] = 1.0f;
	}
	else if (number == 7 ) // Nytrogen - light blue
	{
		color[0] = 143.0/255.0f;
		color[1] = 143.0/255.0f;
		color[2] = 1.0;
	}
	else if (number == 16 ) // Sulfur - yellow
	{
		color[0] = 1.0f;
		color[1] = 200.0/255.0f;
		color[2] = 50.0/255.0f;
	}
	else if (number == 15 || number == 26 ) //Phosphorus/Iron - orange
	{
		color[0] = 255.0/255.0f;
		color[1] = 165.0/255.0f;
		color[2] = 0.0f;
	}
	else if (number == 17) // Chlorine - green
	{
		color[0] = 0.0;
		color[1] = 1.0;
		color[2] = 0.0;
	}
	else if (number == 35 || number == 30) //Bromin/Zin - brown
	{
		color[0] = 165.0/255.0f;
		color[1] = 42.0/255.0f;
		color[2] = 42.0/255.0f;
	}
	else if (number == 11 ) // Sodium - blue
	{
		color[0] = 0.0f;
		color[1] = 0.0f;
		color[2] = 1.0f;
	}
	else if (number == 12 || number == 20 ) // Magnesium Calcium - dark green
	{
		color[0] = 42.0/255.0f;
		color[1] = 128.0/255.0f;
		color[2] = 42.0/255.0f;
	}
	else // other/unknown Pink
	{
		color[0] = 1.0f;
		color[1] = 20.0/255.0f;
		color[2] = 147.0/255.0f;
	}
}
