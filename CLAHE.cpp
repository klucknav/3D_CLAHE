////////////////////////////////////////
// CLAHE.cpp
////////////////////////////////////////

#include "CLAHE.h"


////////////////////////////////////////////////////////////////////////////////
// 2D Helper Methods

// Make LUT 
// to speed up histogram clipping, the input image[minVal...maxVal] (usually[0...255])
// is scaled down to[0, numBins - 1]
// map[0...4096] to[img.min(), img.max()](size = numbins)
// 
// LUT		- to store the Look Up Table
// minVal	- min
// maxVal	- max
// numBins	- number of gray values 
void CLAHE::makeLUT(uint16_t* LUT, uint16_t minVal, uint16_t maxVal, unsigned int numBins) {

	// calculate the size of the bins
	const uint16_t binSize = (uint16_t)(1 + (maxVal - minVal) / numBins);

	// build up the LUT
	for (unsigned int index = minVal; index <= maxVal; index++) {
		// LUT = (i - min) / binSize
		LUT[index] = (index - minVal) / binSize;
	}
}

// build the histogram of the given image and dimensions 
void CLAHE::makeHistogram(uint16_t* subImage, unsigned int uiXRes,
	unsigned int sizeX, unsigned int sizeY, unsigned long* localHist,
	unsigned int uiNrGreylevels, uint16_t* LUT) {

	// clear histogram
	for (unsigned int i = 0; i < uiNrGreylevels; i++) {
		localHist[i] = 0L;
	}

	// build up the histogram
	bit_pixel* ImagePointer;
	for (unsigned int i = 0; i < sizeY; i++) {

		ImagePointer = &subImage[sizeX];

		while (subImage < ImagePointer) {

			localHist[LUT[*subImage++]]++;
		}
		ImagePointer += uiXRes;

		// go to beginning of next row 
		subImage = &ImagePointer[-(int)sizeX];
	}
}



////////////////////////////////////////////////////////////////////////////////

// image	- Pointer to the input/output image
// imgDims	- Image resolution in the X/Y directions
// Min		- Minimum greyvalue of input image (also becomes minimum of output image)
// Max		- Maximum greyvalue of input image (also becomes maximum of output image)
// numCR	- Number of contextial regions in the X/ direction (min 2, max uiMAX_REG_X/Y)
// numBins	- Number of greybins for histogram ("dynamic range")
// cliplimit- Normalized cliplimit (higher values give more contrast)
int CLAHE::CLAHE_2D(uint16_t* image, glm::uvec2 imgDims, uint16_t minVal,
		uint16_t maxVal, glm::uvec2 numCR, unsigned int numBins, float cliplimit) {

	unsigned int uiX, uiY;						// counters
	uint16_t* pImPointer;						// pointer to image

	unsigned long* pulHist;						// pointer to histogram 

	unsigned int uiXL, uiXR, uiYU, uiYB;		// auxiliary variables interpolation routine
	unsigned long* pulLU, * pulLB, * pulRU, * pulRB; // auxiliary pointers interpolation

	// error checking 
	if (numCR.x > _maxNumCR_X) return -1;		// # of regions x-direction too large
	if (numCR.y > _maxNumCR_X) return -2;		// # of regions y-direction too large
	if (imgDims.x % numCR.x) return -3;			// x-resolution no multiple of uiNrX 
	if (imgDims.y % numCR.y) return -4;			// y-resolution no multiple of uiNrY 
	if (maxVal >= NUM_GRAY_VALS) return -5;		// maximum too large
	if (minVal >= maxVal) return -6;			// minimum equal or larger than maximum
	if (numCR.x < 2 || numCR.y < 2) return -7;	// at least 4 contextual regions required
	if (cliplimit == 1.0) return 0;				// is OK, immediately returns original image.
	if (numBins == 0) numBins = 128;			// default value when not specified

	// pointer to mappings
	unsigned long* pulMapArray = (unsigned long*)malloc(sizeof(unsigned long) * numCR.x * numCR.y * numBins);

	// calculate the size of the contextual regions
	unsigned int sizeCRx = imgDims.x / numCR.x;
	unsigned int sizeCRy = imgDims.y / numCR.y;
	unsigned long numPixelsCR = (unsigned long)sizeCRx * (unsigned long)sizeCRy;

	// Make the LUT - to scale the input image
	uint16_t LUT[NUM_GRAY_VALS];
	makeLUT(LUT, minVal, maxVal, numBins);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////