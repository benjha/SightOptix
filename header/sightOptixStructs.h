/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 */
#ifndef SIGHTOPTIXSTRUCTS_H_
#define SIGHTOPTIXSTRUCTS_H_

#include <optixu/optixu_vector_types.h>


typedef struct {
  optix::float4 center; // stores sphere pos. in x, y, z sphere radius in w
  optix::float4 color;
} sightSphereColor;




#endif /* SIGHTOPTIXSTRUCTS_H_ */
