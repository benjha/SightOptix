/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 */

#ifndef LOADERS_H_
#define LOADERS_H_

class cParticles;

int parseAscii (const char* filename, cParticles *p, unsigned int decimation);
std::string getFileExtension(std::string file);
void find_minmax_all(const float *pos, int n, float *min, float *max);

#endif /* LOADERS_H_ */
