////////////////////////////////////////
// CLAHE.h
////////////////////////////////////////

#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <stdio.h>
#include <iostream>

using namespace std;

class ImageLoader;

# define NUM_IN_GRAY_VALS (65536)

class CLAHE {

private:
	// for error checking 
	const unsigned int _maxNumCR_X = 16;
	const unsigned int _maxNumCR_Y = 16;
	const unsigned int _maxNumCR_Z = 16;

	// Image/Volume Properties 
	uint16_t* _imageData;
	unsigned int _imgDimX, _imgDimY, _imgDimZ;
	uint16_t _minVal, _maxVal;


	// CLAHE Helper Methods
	void makeLUT(uint16_t* LUT, unsigned int numBins);

	// Make the local Histograms 2D and 3D
	void makeHistogram2D(uint16_t* image, unsigned int* hist, uint16_t* LUT, 
		unsigned int startX, unsigned int startY, unsigned int endX, unsigned int endY);
	void makeHistogram3D(uint16_t* volume, unsigned int startX, unsigned int startY, unsigned int startZ,
		unsigned int endX, unsigned int endY, unsigned int endZ, unsigned int* hist, uint16_t* LUT);

	// Clip and Map the local Histograms (use same for 2D and 3D)
	void clipHistogram(float clipLimit, unsigned int numBins, unsigned int* localHist);
	void mapHistogram(unsigned int* localHist, unsigned int numBins, unsigned long numPixelsCR);

	// Bilinear and Trilinear Interpolation 
	void lerp2D(uint16_t* image, unsigned int* LU, unsigned int* RU, unsigned int* LD, unsigned int* RD, 
				unsigned int sizeX, unsigned int sizeY, unsigned int currCRx, unsigned int currCRy, uint16_t* LUT, unsigned int numBins);

	void lerp3D(uint16_t* volume, unsigned int * LUF, unsigned int * RUF, unsigned int * LDF, unsigned int * RDF, 
				unsigned int * LUB, unsigned int * RUB, unsigned int * LDB, unsigned int * RDB, 
				unsigned int sizeX, unsigned int sizeY, unsigned int sizeZ,
				unsigned int currSBx, unsigned int currSBy, unsigned int currSBz, uint16_t* LUT, unsigned int numBins);

	// Printing Helper Methods 
	void printHist(unsigned int* hist, unsigned int max);
	void printHist(float* hist, unsigned int max);

public:
	CLAHE(ImageLoader* img);
	CLAHE(uint16_t* img, glm::vec3 imgDims, unsigned int min, unsigned int max);
	~CLAHE();

	int CLAHE_2D(unsigned int numCRx, unsigned int numCRy, unsigned int numBins, float clipLimit);
	int CLAHE_3D(unsigned int numCRx, unsigned int numCRy, unsigned int numCRz, unsigned int numBins, float clipLimit);

};