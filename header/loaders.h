/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 */

#ifndef LOADERS_H_
#define LOADERS_H_

template <typename T> T Max (T x, T y )
{
	return x > y ? x : y;
}

template <typename T> T Min (T x, T y )
{
	return x < y ? x : y;
}

int		loadAscii 	(const char* filename, std::vector<float> *positions, std::vector<float> *nrg, float *min, float *max, unsigned int decimation);
std::string getFileExtension(std::string file);
void find_minmax_all(const float *pos, int n, float *min, float *max);

#endif /* LOADERS_H_ */
