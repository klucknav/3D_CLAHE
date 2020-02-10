////////////////////////////////////////
// CLAHE.cpp
////////////////////////////////////////

#include "CLAHE.h"
#include "ImageLoader.h"

#include <algorithm>


////////////////////////////////////////////////////////////////////////////////
// Constructor/Destructor

CLAHE::CLAHE(ImageLoader* img) {

	glm::vec3 dims = img->GetImageDimensions();

	// Image Properties
	_imageData = img->GetImageData();
	_imgDimX = (unsigned int)dims.x;
	_imgDimY = (unsigned int)dims.y;
	_imgDimZ = (unsigned int)dims.z;

	// GrayValue Properties
	_minVal = (unsigned int)img->GetMinPixelValue();
	_maxVal = (unsigned int)img->GetMaxPixelValue();
	_numBins = NUM_GRAY_VALS;

	// Contextual Region Properties
	_numCRx = _numCRy = 4;
	_clipLimit = 0.85f;

	uint16_t max = 0;
	uint16_t min = NUM_GRAY_VALS;
	for (uint16_t i = 0; i < _imgDimX; i++) {
		for (uint16_t j = 0; j < _imgDimY; j++) {
			uint16_t val = _imageData[i * _imgDimX + _imgDimY];
			if (val > max) {
				max = val;
			}
			else if (val < min) {
				min = val;
			}
		}
	}

	printf("CLAHE: \n");
	printf("max = %d, min = %d\n", max, min);
}

CLAHE::CLAHE(bit_pixel* img, glm::vec3 imgDims, unsigned int min,
	unsigned int max) {

	// Image Properties 
	_imageData = img;
	_imgDimX = (unsigned int)imgDims.x;
	_imgDimY = (unsigned int)imgDims.y;
	_imgDimZ = (unsigned int)imgDims.z;

	// GrayValue Properties
	_minVal = min;
	_maxVal = max;
	_numBins = NUM_GRAY_VALS;

	// Contextual Region Properties
	_numCRx = _numCRy = 4;
	_clipLimit = 0.85f;

	printf("CLAHE: image size: (%i, %i)\n", _imgDimX, _imgDimY);
}

CLAHE::~CLAHE() {

}


////////////////////////////////////////////////////////////////////////////////
// 2D Helper Methods

// Make LUT 
// to speed up histogram clipping, the input image[minVal...maxVal] (usually[0...255])
// is scaled down to[0, numBins - 1]
// map[0...4096] to[img.min(), img.max()](size = numbins)
// 
// LUT		- to store the Look Up Table
// minVal	- min gray value from teh DICOM image
// maxVal	- max gray value from teh DICOM image
// numBins	- number of gray values we can use
void CLAHE::makeLUT(bit_pixel* LUT, unsigned int numBins) {

	// calculate the size of the bins
	const uint16_t binSize = (uint16_t)(1 + ((_maxVal - _minVal) / numBins));

	// build up the LUT
	for (unsigned int index = _minVal; index <= _maxVal; index++) {
		// LUT = (i - min) / binSize
		LUT[index] = (index - _minVal) / binSize;
	}
}

// build the histogram of the given image and dimensions 
// subImage  - pointer to the image
// sizeCRx   - size of the critical section in the x direction
// sizeCRy   - size of the critical section in the y direction
// localHist - histogram for this critical section
// numBins	 - number of gray values
// LUT		 - 
void CLAHE::makeHistogram(bit_pixel* subImage, unsigned int sizeCRx, unsigned int sizeCRy,
	unsigned long* localHist, unsigned int numBins, uint16_t* LUT) {

	// clear histogram for each of the gray values 
	for (unsigned int i = 0; i < numBins; i++) {
		localHist[i] = 0L;
	}

	// build up the histogram
	bit_pixel* imagePointer;	// pointer to the image data
	for (unsigned int i = 0; i < sizeCRy; i++) {

		// process the row 
		imagePointer = &subImage[sizeCRx];
		while (subImage < imagePointer) {
			// get color from the CR(sub_img) 
			// - map it to the values used (LUT)
			// - increment the histogram for that grayvalue
			uint16_t index1 = *subImage++; // wrong
			uint16_t index2 = LUT[index1];
			cerr << "color: " << index1 << " newColor: " << index2 << endl;
			localHist[index2] += 1;	// ISSUE HERE 
			//localHist[ LUT[*subImage++] ]++;
		}
		// go to beginning of next row 
		imagePointer += _imgDimX;
		subImage = &imagePointer[-(int)sizeCRx];
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
int CLAHE::CLAHE_2D(unsigned int numCRx, unsigned int numCRy, unsigned int numBins, float clipLimit) {

	//unsigned long* pulHist;					// pointer to histogram 
	//unsigned int uiXL, uiXR, uiYU, uiYB;		// auxiliary variables interpolation routine
	//unsigned long* pulLU, * pulLB, * pulRU, * pulRB; // auxiliary pointers interpolation

	// error checking 
	if (numCRx > _maxNumCR_X) return -1;		// # of regions x-direction too large
	if (numCRy > _maxNumCR_X) return -2;		// # of regions y-direction too large
	if (_imgDimX % numCRx) return -3;			// x-resolution no multiple of uiNrX 
	if (_imgDimY % numCRy) return -4;			// y-resolution no multiple of uiNrY 
	if (clipLimit == 1.0) return 0;				// is OK, immediately returns original image.
	if (numBins == 0) numBins = NUM_GRAY_VALS;	// default value when not specified


	// calculate the size of the contextual regions
	unsigned int sizeCRx = _imgDimX / numCRx;
	unsigned int sizeCRy = _imgDimY / numCRy;
	unsigned long numPixelsCR = (unsigned long)sizeCRx * (unsigned long)sizeCRy;

	printf("Num Contextual Regions: %i, %i, Image size: %i, %i\n", numCRx, numCRy, _imgDimX, _imgDimY);
	printf("Size of Contextual Regions: %d, %d, each with %d pixels\n\n", sizeCRx, sizeCRy, numPixelsCR);
	printf("Make LUT...Image Range: [%d, %d]\n", _minVal, _maxVal);

	// Make the LUT - to scale the input image
	uint16_t* LUT = new uint16_t[_maxVal - _minVal];
	makeLUT(LUT, numBins);

	// pointer to mappings (bins)
	unsigned long* currHist;
	unsigned long* mappedHist = (unsigned long*)malloc(sizeof(unsigned long) * numCRx * numCRy * numBins);

	// Calculate greylevel mappings for each contextual region 
	printf("build local histograms...");
	uint16_t* imgPointer = _imageData;
	for (unsigned int currCRy = 0; currCRy < numCRy; currCRy++) {
		unsigned int startY = currCRy * sizeCRy;
		unsigned int endY = (currCRy + 1) * sizeCRy;

		for (unsigned int currCRx = 0; currCRx < numCRx; currCRx++, imgPointer += sizeCRx) {
			unsigned int startX = currCRx * sizeCRx;
			unsigned int endX = (currCRx + 1) * sizeCRx;

			// calculate the local histogram for the given contextual region
			currHist = &mappedHist[numBins * (currCRy * numCRx + currCRx)];
			makeHistogram(imgPointer, sizeCRx, sizeCRy, currHist, numBins, LUT);

			//// clip the histogram
			//clipHistogram(histogram, numBins, clipLimit);

			//// calculate the cumulative histogram and re-map to [min, max]
			//mapHistogram(histogram, minVal, maxVal, numBins, numPixelsCR);
		}
		// skip lines, set pointer
		imgPointer += (sizeCRy - 1) * _imgDimX;
	}

	cerr << endl;

	// clean up 
	delete[] LUT;
	return 0;
}

////////////////////////////////////////////////////////////////////////////////