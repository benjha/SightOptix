/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 */
#ifndef GLOBALS_H_
#define GLOBALS_H_

// OSPRay and Optix work better if
// geometry is split into groups
#define NUM_GROUPS		1024
#define PARTICLES_PER_GROUP	1024*1024

template <typename T> T mAX (T a, T b )
{
	return a > b ? a : b;
}

template <typename T> T mIN (T a, T b )
{
	return a < b ? a : b;
}


#endif /* GLOBALS_H_ */
