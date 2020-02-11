////////////////////////////////////////
// CLAHE.h
////////////////////////////////////////

#pragma once

#include <cstdint>
#include <stdio.h>
#include <iostream>

#include <glm/glm.hpp>


using namespace std;


// 8 bit-per-pixel images
//#ifdef BYTE_IMAGE
//typedef uint16_t bit_pixel;
//#define NUM_GRAY_VALS (256)

//#else
// 12 bit-per-pixel images
typedef uint16_t bit_pixel;
# define NUM_GRAY_VALS (4096)
//#endif

class ImageLoader;

class CLAHE {
private:
	// for error checking 
	const unsigned int _maxNumCR_X = 16;
	const unsigned int _maxNumCR_Y = 16;


	// Image Properties 
	//ImageLoader* _loadedImage;
	bit_pixel* _imageData;
	unsigned int _imgDimX, _imgDimY, _imgDimZ;

	// Grayvalue Properties 
	unsigned int _minVal, _maxVal, _numBins;

	// Contextual Region Properties 
	unsigned int _numCRx, _numCRy;

	// Contrast Limited Value
	float _clipLimit;


	// Helper Methods
	void makeLUT(bit_pixel* LUT, unsigned int numBins);

	void makeHistogram(bit_pixel* subImage, unsigned int sizeCRx, unsigned int sizeCRy,
						unsigned long* localHist, unsigned int numBins, uint16_t* LUT);

	void clipHistogram(float clipLimit, unsigned int numBins, unsigned long* localHist);

	void mapHistogram(unsigned long* localHist, unsigned int numBins, unsigned long numPixelsCR);

	void lerp_2D(uint16_t* image, unsigned long* LU, unsigned long* RU, unsigned long* LD, 
				unsigned long* RD, unsigned int sizeX, unsigned int sizeY, uint16_t* LUT);

public:
	CLAHE(ImageLoader* img);
	CLAHE(bit_pixel* img, glm::vec3 imgDims, unsigned int min, unsigned int max);
	~CLAHE();

	int CLAHE_2D(unsigned int numCRx, unsigned int numCRy, unsigned int numBins, float clipLimit);
	//void CLAHE_3D();

};