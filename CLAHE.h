////////////////////////////////////////
// CLAHE.h
////////////////////////////////////////

#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <stdio.h>
#include <iostream>

using namespace std;


// 8 bit-per-pixel images
//#ifdef BYTE_IMAGE
//typedef uint16_t bit_pixel;
//#define NUM_GRAY_VALS (256)

//#else
// 12 bit-per-pixel images
typedef uint16_t bit_pixel;
//# define NUM_GRAY_VALS (4096)
# define NUM_IN_GRAY_VALS (65536)
//#endif

class ImageLoader;

class CLAHE {
private:
	// for error checking 
	const unsigned int _maxNumCR_X = 16;
	const unsigned int _maxNumCR_Y = 16;
	const unsigned int _maxNumCR_Z = 16;

	// Image Properties 
	//ImageLoader* _loadedImage;
	uint16_t* _imageData;
	unsigned int _imgDimX, _imgDimY, _imgDimZ;

	// Grayvalue Properties 
	unsigned int _minVal, _maxVal, _numBins;

	// Contextual Region Properties 
	unsigned int _numCRx, _numCRy;

	// Contrast Limited Value
	float _clipLimit;


	// Helper Methods
	void makeLUT(bit_pixel* LUT, unsigned int numBins);

	void makeHistogram2D(bit_pixel* subImage, unsigned int sizeCRx, unsigned int sizeCRy,
						unsigned int* localHist, unsigned int numBins, uint16_t* LUT);
	void makeHistogram3D_v2(uint16_t* subVolume, unsigned int sizeSBx, unsigned int sizeSBy,
						unsigned int sizeSBz, unsigned int* localHist, unsigned int numBins, uint16_t* LUT);
	void makeHistogram3D(uint16_t* volume, unsigned int startX, unsigned int startY, unsigned int startZ,
		unsigned int endX, unsigned int endY, unsigned int endZ, unsigned int* hist, uint16_t* LUT);

	void clipHistogram(float clipLimit, unsigned int numBins, unsigned int* localHist);
	void clipHistogram3D(unsigned int* localHist, unsigned int startX, unsigned int startY, unsigned int startZ,
		unsigned int endX, unsigned int endY, unsigned int endZ, unsigned int numBins, float clipLimit);

	void mapHistogram(unsigned int* localHist, unsigned int numBins, unsigned long numPixelsCR);

	void lerp2D(uint16_t* image, unsigned int* LU, unsigned int* RU, unsigned int* LD, 
				unsigned int* RD, unsigned int sizeX, unsigned int sizeY, uint16_t* LUT, unsigned int numBins);

	void lerp3D(uint16_t* volume, unsigned int* LUF, unsigned int* RUF, unsigned int* LDF,
				unsigned int* RDF, unsigned int* LUB, unsigned int* RUB, unsigned int* LDB,
				unsigned int* RDB, unsigned int sizeX, unsigned int sizeY, unsigned int sizeZ,
				unsigned int currSBx, unsigned int currSBy, unsigned int currSBz, uint16_t* LUT, unsigned int numBins);

	void printHist(unsigned int* hist, unsigned int max);

public:
	CLAHE(ImageLoader* img);
	CLAHE(uint16_t* img, glm::vec3 imgDims, unsigned int min, unsigned int max);
	~CLAHE();

	int CLAHE_2D(unsigned int numCRx, unsigned int numCRy, unsigned int numBins, float clipLimit);
	int CLAHE_3D(unsigned int numCRx, unsigned int numCRy, unsigned int numCRz, unsigned int numBins, float clipLimit);

};