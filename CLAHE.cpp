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
	_clipLimit = 0.85f * 0.75f;

	uint16_t max = 0;
	uint16_t min = NUM_GRAY_VALS;
	for (uint16_t i = 0; i < _imgDimX; i++) {
		for (uint16_t j = 0; j < _imgDimY; j++) {
			uint16_t val = _imageData[j * _imgDimX + i];
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
	_minVal = min;
	_maxVal = max;
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
	//cerr << "binSize: " << binSize << " numBins: " << numBins << endl;
	//cerr << "_maxVal: " << _maxVal << " _minVal: " << _minVal << endl;

	// build up the LUT
	for (unsigned int index = _minVal; index <= _maxVal; index++) {
		// LUT = (i - min) / binSize
		LUT[index] = (index - _minVal) / binSize;
		//cerr << "(index: " << index << " val: " << (index - _minVal) / binSize << ")\n";
	}
	//cerr << "LUT end\n";
}

// build the histogram of the given image and dimensions 
// subImage  - pointer to the image
// sizeCRx   - size of the critical section in the x direction
// sizeCRy   - size of the critical section in the y direction
// localHist - histogram for this critical section
// numBins	 - number of gray values
// LUT		 - mapping from image's gray values to the desired gray value range
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
			localHist[ LUT[*subImage++] ]++;

			//uint16_t index1 = *subImage++;
			//uint16_t index2 = LUT[index1];
			//cerr << "color: " << index1 << " newColor: " << index2 << endl;
			//localHist[index2] += 1;	
		}
		// go to beginning of next row 
		imagePointer += _imgDimX;
		subImage = &imagePointer[-(int)sizeCRx];
	}
}


// Clip the Histogram
// clipLimit - normalized clip limit [0, 1]
// numBins   - number of gray values in the final image
// localHist - histogram for the current critical section
void CLAHE::clipHistogram(float clipLimit, unsigned int numBins, unsigned long* localHist) {
	
	// calculate the clipValue based on the max of the localHist:
	float max = 0.0f;
	unsigned long* binPointer1 = localHist;
	for (unsigned int i = 0; i < numBins; i++) {
		float value = (float)binPointer1[i];
		if (value > max) {
			max = value;
		}
	}
	float clipValue = (float)clipLimit * max;
	//printf("max: %f, clipValue: %f\n", max, clipValue);

	// calculate the total number of excess pixels
	unsigned long excess = 0.0;			
	unsigned long * binPointer = localHist;
	for (unsigned int i = 0; i < numBins; i++) {
		long binExcess = (long)binPointer[i] - (long)clipValue;
		if (binExcess > 0.0) {
			excess += binExcess;
		}
	}
	//cerr << "1. number of excess: " << excess << endl;

	// Clip histogram and redistribute excess pixels in each bin 
	unsigned long avgInc = excess / numBins;
	unsigned long upper = clipValue - avgInc;	// Bins larger than upper set to clipValue
	for (unsigned int i = 0; i < numBins; i++) {
		// clip the bin
		if (localHist[i] > clipValue) {
			localHist[i] = clipValue;
		}
		else {
			// if the value is too large remove from the bin into excess 
			if (localHist[i] > upper) {
				excess -= localHist[i] - upper; 
				localHist[i] = clipValue;
			}
			// otherwise put the excess into the bin
			else {
				excess -= avgInc; 
				localHist[i] += avgInc;
			}
		}
	}
	//cerr << "2. number of excess: " << excess << endl;

	// Redistribute the remaining excess pixels
	while (excess) { 
		unsigned long* endPointer = &localHist[numBins];
		unsigned long* histPointer = localHist;
		while (excess && histPointer < endPointer) {

			unsigned long stepSize = numBins / excess;
			// make sure the stepsize is at least 1
			if (stepSize < 1) {
				stepSize = 1;
			}

			// add excess to the hist 
			for (binPointer = localHist; binPointer < endPointer && excess; binPointer += stepSize) {
				if (*binPointer < clipValue) {
					(*binPointer)++;
					excess--;
				}
			}
			//cerr << "3. number of excess: " << excess << endl;
			// restart redistributing on other bin location
			histPointer++;
		}
	}

}


// Map the Histogram
// calculate the equalized lookup table (mapping) by cqlculating the CDF 
// NOTE: lookup table is rescaled in range[min...max]
// Max and Min are same as for the LUT creation
void CLAHE::mapHistogram(unsigned long* localHist, unsigned int numBins, unsigned long numPixelsCR) {

	unsigned long sum = 0;
	const float scale = ((float)(_maxVal - _minVal)) / numPixelsCR;

	// for each bin
	for (unsigned int i = 0; i < numBins; i++) {

		// add the histogram value for this contextual region to the sum 
		sum += localHist[i]; 

		// to normalize the cdf
		localHist[i] = (unsigned long)(min(_minVal + sum * scale, (float)_maxVal));
	}
}

// Lerp in 2D
// LU, RU, LB, RB - loal mapped histograms 
// sizeX, sizeY   - size of subImage we are interpolating over
// LUT			  - mapping from image's gray values to the desired gray value range
void CLAHE::lerp2D(uint16_t* imgPointer, unsigned long* LU, unsigned long* RU,
	unsigned long* LB, unsigned long* RB, unsigned int sizeX, unsigned int sizeY, uint16_t* LUT) {
	
	// Normalization factor
	unsigned int normFactor = sizeX * sizeY;

	// for each pixel in the subImage
	for (unsigned int a = 0; a < sizeY; a++, imgPointer += _imgDimX - sizeX) {
		unsigned int aInv = sizeY - a;

		for (unsigned int b = 0;  b < sizeX; b++) {
			unsigned int bInv = sizeX - b;

			uint16_t greyValue = LUT[*imgPointer];

			// bilinearly interpolate the mapped histograms (LU, RU, LB, RB)
			unsigned int p1 = bInv * LU[greyValue] + b * RU[greyValue];
			unsigned int p2 = bInv * LB[greyValue] + b * RB[greyValue];
			unsigned int ans = ((aInv * p1 + a * p2) / normFactor);

			*imgPointer++ = (uint16_t)ans;

			//*imgPointer++ = (uint16_t)((aInv * (bInv * LU[greyValue] + b * RU[greyValue])
			//							+ a  * (bInv * LB[greyValue] + b * RB[greyValue])) / normFactor);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// 2D version of CLAHE
//
// image	- Pointer to the input/output image
// imgDims	- Image resolution in the X/Y directions
// Min		- Minimum greyvalue of input image (also becomes minimum of output image)
// Max		- Maximum greyvalue of input image (also becomes maximum of output image)
// numCR	- Number of contextial regions in the X/ direction (min 2, max uiMAX_REG_X/Y)
// numBins	- Number of greybins for histogram ("dynamic range")
// cliplimit- Normalized cliplimit (higher values give more contrast)
int CLAHE::CLAHE_2D(unsigned int numCRx, unsigned int numCRy, unsigned int numBins, float clipLimit) {

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
	printf("Make LUT...Image Range: [%d, %d], numBins: %d\n", _minVal, _maxVal, numBins);

	// Make the LUT - to scale the input image
	//cerr << "range: " << _maxVal - _minVal << endl;
	uint16_t* LUT = new uint16_t[NUM_GRAY_VALS];
	makeLUT(LUT, numBins); // <- issue

	// pointer to mappings (bins)
	unsigned long* currHist;
	unsigned long* mappedHist = (unsigned long*)malloc(sizeof(unsigned long) * numCRx * numCRy * numBins);

	// Calculate greylevel mappings for each contextual region 
	printf("build local histograms...\n");
	uint16_t* imgPointer = _imageData;
	for (unsigned int currCRy = 0; currCRy < numCRy; currCRy++) {

		for (unsigned int currCRx = 0; currCRx < numCRx; currCRx++, imgPointer += sizeCRx) {

			// calculate the local histogram for the given contextual region
			currHist = &mappedHist[numBins * (currCRy * numCRx + currCRx)];
			makeHistogram(imgPointer, sizeCRx, sizeCRy, currHist, numBins, LUT);

			// clip the histogram
			clipHistogram(clipLimit, numBins, currHist);

			// calculate the cumulative histogram and re-map to [min, max]
			mapHistogram(currHist, numBins, numPixelsCR);
		}
		// skip lines, set pointer
		imgPointer += (sizeCRy - 1) * _imgDimX;
	}

	
	// Interpolate greylevel mappings to get CLAHE image 
	printf("interpolate...\n");
	imgPointer = _imageData;
	unsigned int xRight, xLeft, yUp, yDown, subY, subX;
	unsigned long* L_Up, * L_Dn, * R_Up, * R_Dn; 
	for (unsigned int currCRy = 0; currCRy <= numCRy; currCRy++) {

		// top row
		if (currCRy == 0) {	
			subY = sizeCRy / 2;  
			yUp = 0;			yDown = 0;
		}
		else {
			// bottom row
			if (currCRy == numCRy) {
				subY = (sizeCRy + 1) / 2;
				yUp = numCRy - 1;			yDown = yUp;
			}
			// default values
			else {
				subY = sizeCRy;
				yUp = currCRy - 1;			yDown = currCRy;
			}
		}
		for (unsigned int currCRx = 0; currCRx <= numCRx; currCRx++) {

			// left column
			if (currCRx == 0) {
				subX = sizeCRx / 2; 
				xLeft = 0;			xRight = 0;
			}
			else {
				// special case: right column
				if (currCRx == numCRx) {
					subX = (sizeCRx + 1) / 2;
					xLeft = numCRx - 1;				xRight = xLeft;
				}
				// default values
				else {
					subX = sizeCRx;
					xLeft = currCRx - 1;			xRight = currCRx;
				}
			}

			L_Up = &mappedHist[numBins * (yUp   * numCRx + xLeft )];
			R_Up = &mappedHist[numBins * (yUp   * numCRx + xRight)];
			L_Dn = &mappedHist[numBins * (yDown * numCRx + xLeft )];
			R_Dn = &mappedHist[numBins * (yDown * numCRx + xRight)];

			cerr << "L_up: " << numBins * (yUp * numCRx + xLeft) << endl;
			cerr << "R_up: " << numBins * (yUp * numCRx + xRight) << endl;
			cerr << "L_dn: " << numBins * (yDown * numCRx + xLeft) << endl;
			cerr << "R_dn: " << numBins * (yDown * numCRx + xRight) << endl;
			cerr << "-----\n";
			lerp2D(imgPointer, L_Up, R_Up, L_Dn, R_Dn, subX, subY, LUT);

			imgPointer += subX;
		}
		imgPointer += (subY - 1) * _imgDimX;
	}

	// clean up 
	free(mappedHist);
	delete[] LUT;

	cerr << "fin\n";

	// store the new data as a texture 
	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, _imgDimX, _imgDimY, 0, GL_RED, GL_UNSIGNED_SHORT, (void*)_imageData);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindTexture(GL_TEXTURE_2D, 0);
	return textureID;
}

////////////////////////////////////////////////////////////////////////////////