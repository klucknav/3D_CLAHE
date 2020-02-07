////////////////////////////////////////
// CLAHE.h
////////////////////////////////////////

#pragma once

#include <cstdint>

#include <glm/glm.hpp>


#ifdef BYTE_IMAGE
typedef unsigned char bit_pixel;	 /* for 8 bit-per-pixel images */
#define NUM_GRAY_VALS (256)
#else
typedef uint16_t bit_pixel;	 /* for 12 bit-per-pixel images (default) */
# define NUM_GRAY_VALS (4096)
#endif


class CLAHE {
private:

	const unsigned int _maxNumCR_X = 16;
	const unsigned int _maxNumCR_Y = 16;


	static void makeLUT(uint16_t* LUT, uint16_t minVal, uint16_t maxVal, unsigned int numBins);

	static void makeHistogram(uint16_t*, unsigned int, unsigned int, unsigned int,
		unsigned long*, unsigned int, uint16_t*);

	static void ClipHistogram(unsigned long*, unsigned int, unsigned long);

	static void MapHistogram(unsigned long*, bit_pixel, bit_pixel,
		unsigned int, unsigned long);
	
	static void Interpolate(bit_pixel*, int, unsigned long*, unsigned long*,
		unsigned long*, unsigned long*, unsigned int, unsigned int, bit_pixel*);
public:

	int CLAHE_2D(uint16_t* image, glm::uvec2 imgDims, uint16_t minVal,
		uint16_t maxVal, glm::uvec2 numCR, unsigned int numBins, float cliplimit);

	void CLAHE_3D();

};