////////////////////////////////////////
// CLAHE.cpp
////////////////////////////////////////

#include "CLAHE.h"
#include "ImageLoader.h"

#include <algorithm>
#include <chrono>


////////////////////////////////////////////////////////////////////////////////
// Texture Helpers 

GLuint initTexture2D(unsigned int width, unsigned int height,
	GLenum internalFormat, GLenum format, GLenum type, GLenum filter, void* data) {

	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

	glBindTexture(GL_TEXTURE_2D, 0);

	return textureID;
}

GLuint initTexture3D(unsigned int width, unsigned int height, unsigned int depth,
	GLenum internalFormat, GLenum format, GLenum type, GLenum filter, void* data) {

	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_3D, textureID);
	glTexImage3D(GL_TEXTURE_3D, 0, internalFormat, width, height, depth, 0, format, type, data);

	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, filter);

	glBindTexture(GL_TEXTURE_3D, 0);

	return textureID;
}


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
	uint16_t max = 0;
	uint16_t min = (uint16_t)NUM_IN_GRAY_VALS;
	for (unsigned int i = 0; i < _imgDimX * _imgDimY * _imgDimZ; i++) {
		uint16_t val = _imageData[i];
		if (val > max) {
			max = val;
		}
		else if (val < min) {
			min = val;
		}
	}
	_minVal = min;
	_maxVal = max;

	printf("CLAHE: \n");
	printf("imageDims: %d, %d, %d\n", _imgDimX, _imgDimY, _imgDimZ);
	printf("min = %d, max = %d\n", _minVal, _maxVal);
}

CLAHE::CLAHE(uint16_t* img, glm::vec3 imgDims, unsigned int minV, unsigned int maxV) {

	// Image Properties 
	_imageData = img;
	_imgDimX = (unsigned int)imgDims.x;
	_imgDimY = (unsigned int)imgDims.y;
	_imgDimZ = (unsigned int)imgDims.z;

	// GrayValue Properties
	_minVal = minV;
	_maxVal = maxV;

	uint16_t max = 0;
	uint16_t min = (uint16_t)NUM_IN_GRAY_VALS;
	for (unsigned int i = 0; i < _imgDimX * _imgDimY * _imgDimZ; i++) {
		uint16_t val = _imageData[i];
		if (val > max) {
			max = val;
		}
		else if (val < min) {
			min = val;
		}
	}

	printf("CLAHE: \n");
	printf("imageDims: %d, %d, %d\n", _imgDimX, _imgDimY, _imgDimZ);
	printf("min = %d, max = %d\n", min, max);
}

CLAHE::~CLAHE() {

}


////////////////////////////////////////////////////////////////////////////////
// CLAHE Helper Methods

/////// MAKE LUT ///////
// to speed up histogram clipping, the input image[minVal...maxVal] (usually[0...255])
// is scaled down to[0, numBins - 1]
// map[0...4096] to[img.min(), img.max()](size = numbins)
// 
// LUT		- to store the Look Up Table
// minVal	- min gray value from teh DICOM image
// maxVal	- max gray value from teh DICOM image
// numBins	- number of gray values we can use
void CLAHE::makeLUT(uint16_t* LUT, unsigned int numBins) {

	// calculate the size of the bins
	const uint16_t binSize = (uint16_t)(1 + ((_maxVal - _minVal) / numBins));
	//cerr << "binSize: " << binSize << " numBins: " << numBins << endl;
	//cerr << "_maxVal: " << _maxVal << " _minVal: " << _minVal << endl;
	//cerr << "LUT:\n";

	// build up the LUT
	for (unsigned int index = _minVal; index <= _maxVal; index++) {
		// LUT = (i - min) / binSize
		LUT[index] = (index - _minVal) / binSize;
		//cerr << "(index: " << index << " val: " << (index - _minVal) / binSize << ")\n";
	}
	//cerr << "LUT end\n";
}


/////// MAKE HISTOGRAM 2D ///////
// build the histogram of the given image and dimensions 
// image     - image data
// sizeCR_   - size of the critical section in the _ direction
// localHist - histogram for this critical section
// numBins	 - number of gray values
// LUT		 - mapping from image's gray values to the desired gray value range
void CLAHE::makeHistogram2D(uint16_t* image, unsigned int* localHist, uint16_t* LUT,
	unsigned int startX, unsigned int startY, unsigned int endX, unsigned int endY, unsigned int xDim) {

	// cycle through the section of the image 
	for (unsigned int currY = startY; currY < endY; currY++) {
		for (unsigned int currX = startX; currX < endX; currX++) {

			// increment the localHist for the image grayvalue 
			localHist[LUT[image[currY * xDim + currX]]]++;
		}
	}
}

/////// MAKE HISTOGRAM 3D ///////
// build the histogram of the given volume and dimensions 
// subImage  - pointer to the volume
// sizeSB_   - size of the subBlock in the _ direction
// localHist - histogram for this subBlock
// numBins	 - number of gray values
// LUT		 - mapping from image's gray values to the desired gray value range
void CLAHE::makeHistogram3D(uint16_t* volume, unsigned int startX, unsigned int startY, unsigned int startZ,
	unsigned int endX, unsigned int endY, unsigned int endZ, unsigned int* localHist, uint16_t* LUT, unsigned int xDim, unsigned int yDim) {

	// cycle over the subBlock of the volume 
	for (unsigned int currZ = startZ; currZ < endZ; currZ++) {
		for (unsigned int currY = startY; currY < endY; currY++) {
			for (unsigned int currX = startX; currX < endX; currX++) {

				// increment the localHist for the volume grayValue 
				localHist[LUT[volume[currZ * xDim * yDim + currY * xDim + currX]]]++;
			}
		}
	}
}


/////// CLIP HISTOGRAM ///////
// clipLimit - normalized clip limit [0, 1]
// numBins   - number of gray values in the final image
// localHist - histogram for the current critical section
void CLAHE::clipHistogram(float clipLimit, unsigned int numBins, unsigned int* localHist) {
	
	// calculate the clipValue based on the max of the localHist:
	unsigned int max = 0;
	unsigned int* hist = localHist;
	for (unsigned int i = 0; i < numBins; i++) {
		unsigned int value = hist[i];
		if (value > max) {
			max = value;
		}
	}
	//float clipValue = (float)clipLimit * (float) max;
	unsigned int clipValue = (unsigned int)( (float)clipLimit * max);
	//printf("max: %u\tclipValue: %5u\t", max, clipValue);
	//float clipValue = (float)clipLimit * _maxVal;
	//printf("max: %u, clipValue: %f\n", _maxVal, clipValue);


	// calculate the total number of excess pixels
	unsigned int excess = 0;
	hist = localHist;
	uint32_t count = 0;
	for (unsigned int i = 0; i < numBins; i++) {
		int toRemove = hist[i] - clipValue;
		if (toRemove > 0) {
			excess += toRemove;
			count++;
		}
	}
	//printf("excess: %lu\tcount: %u\n", excess, count);
	//printf("excess: %lu\t", excess);
	//cerr << "1. number of excess: " << excess << " count: " << count << endl;

	// Clip histogram and redistribute excess pixels in each bin 
	unsigned int avgInc = excess / numBins;
	unsigned int upper = clipValue - avgInc;	// Bins larger than upper set to clipValue
	//printf("avgInc: %u\tupper: %u\t", avgInc, upper);
	for (unsigned int i = 0; i < numBins; i++) {
		// if the number in the histogram is too big -> clip the bin
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
	//printf("excess: %lu\t", excess);
	//cerr << "2. number of excess: " << excess << endl;

	// Redistribute the remaining excess pixels
	while (excess > 0) { 
		unsigned int* endPointer = &localHist[numBins];
		unsigned int* histPointer = localHist;
		while (excess > 0 && histPointer < endPointer) {

			unsigned long stepSize = (unsigned long) (numBins / excess);
			//printf("stepSize:  %lu\t", stepSize);
			// make sure the stepsize is at least 1
			if (stepSize < 1) {
				stepSize = 1;
			}

			// add excess to the hist 
			for (hist = localHist; hist < endPointer && excess; hist += stepSize) {
				if (*hist < clipValue) {
					(*hist)++;
					excess--;
				}
			}
			//cerr << "3. number of excess: " << excess << endl;
			// restart redistributing on other bin location
			histPointer++;
			//printf("excess: %lu\t", excess);
		}
	}
	//printf("\n");

}


/////// MAP HISTOGRAM ///////
// calculate the equalized lookup table (mapping) by calculating the CDF 
// NOTE: lookup table is rescaled in range[min...max]
// Max and Min are same as for the LUT creation
void CLAHE::mapHistogram(unsigned int* localHist, unsigned int numBins, unsigned long numPixelsCR) {

	float sum = 0;
	const float scale = ((float)(_maxVal - _minVal)) / (float) numPixelsCR;
	//printf("min: %u, \tmax: %u, \tnumPixels: %u, \tnumBins: %u, scale: %f\n", _minVal, _maxVal, numPixelsCR, numBins, scale);

	// for each bin
	for (unsigned int i = 0; i < numBins; i++) {

		// add the histogram value for this contextual region to the sum 
		sum += localHist[i]; 

		// normalize the cdf
		localHist[i] = (unsigned int)(min(_minVal + sum * scale, (float)_maxVal));
	}
}


/////// LERP - 2D ///////
// LU, RU, LB, RB - loal mapped histograms 
// sizeX, sizeY   - size of subImage we are interpolating over
// LUT			  - mapping from image's gray values to the desired gray value range
void CLAHE::lerp2D(uint16_t* image, unsigned int* LU, unsigned int* RU, unsigned int* LB, unsigned int* RB, 
	unsigned int sizeX, unsigned int sizeY, unsigned int startX, unsigned int startY, uint16_t* LUT, unsigned int numBins, unsigned int xDim) {
	
	float normFactor = (float)(sizeX * sizeY);
	//normFactor /= ((float)numBins / (float)_maxVal);

	// for each pixel in the subImage
	for (unsigned int b = 0; b < sizeY; b++) {
		unsigned int bInv = sizeY - b;
		unsigned int currY = startY + b;

		for (unsigned int a = 0;  a < sizeX; a++) {
			unsigned int aInv = sizeX - a;
			unsigned int currX = startX + a;

			uint16_t greyValue = LUT[image[currY * xDim + currX]];

			// bilinearly interpolate the mapped histograms (LU, RU, LB, RB)
			unsigned int p1 = aInv * LU[greyValue] + a * RU[greyValue];
			unsigned int p2 = aInv * LB[greyValue] + a * RB[greyValue];
			unsigned int ans = ((bInv * p1 + b * p2) / (unsigned int)normFactor);

			image[currY * xDim + currX] = (uint16_t)ans;
		}
	}
}


/////// LERP - 3D ///////
// volume			   - pointer to the volume 
// LUF, RUF, LDF, RDF  - loal mapped histograms (front)
// LUB, RUB, LDB, RDB  - loal mapped histograms (back)
// sizeX, sizeY, sizeZ - size of the subVolume/subBlock we are interpolating over 
// start_		       - index in _ dim that we are starting at
// LUT				   - mapping from volume's gray values to the desired gray value range
void CLAHE::lerp3D(uint16_t* volume, 
	unsigned int * LUF, unsigned int * RUF, unsigned int * LDF, unsigned int * RDF, 
	unsigned int * LUB, unsigned int * RUB, unsigned int * LDB, unsigned int * RDB, 
	unsigned int sizeX, unsigned int sizeY, unsigned int sizeZ, unsigned int startX, unsigned int startY, unsigned int startZ, 
	uint16_t* LUT, unsigned int numBins, unsigned int xDim, unsigned int yDim) {

	// Normalization factor
	float normFactor = (float)sizeX * (float)sizeY * (float)sizeZ;
	//normFactor /= ((float)numBins / (float)_maxVal);	// for downsizing the number of gray values

	//printf("-----\n3D lerp\n"); 
	//printf("StartingIndex: (%d, %d, %d)\t", startX, startY, startZ);
	//printf("sizeSB: (%d, %d, %d)\n", sizeX, sizeY, sizeZ);
	//printf("normalization factor: %f\n", normFactor);

	// for each voxel in the subVolume
	float maxVal = 0.0f;
	for (unsigned int c = 0; c < sizeZ; c++) {
		unsigned int cInv = sizeZ - c;
		unsigned int currZ = startZ + c;

		for (unsigned int b = 0; b < sizeY; b++) {
			unsigned int bInv = sizeY - b;
			unsigned int currY = startY + b;

			for (unsigned int a = 0; a < sizeX; a++) {
				unsigned int aInv = sizeX - a;
				unsigned int currX = startX + a;

				// get the previous greyValue of the volume (converted to the desired range via the LUT)
				int volIndex = currZ * xDim * yDim + currY * xDim + currX;
				uint16_t greyValue = LUT[volume[volIndex]];

				//printf("volume index: %d\n", volIndex);
				//printf("a: %d, b: %d, c: %d\taInv: %d, bInv: %d, cInv: %d\n", a, b, c, aInv, bInv, cInv);
				//cerr << "LUF " << LUF[greyValue] << " : " << "RUF " << RUF[greyValue] << " : " << "LDF " << LDF[greyValue] << " : " << "RDF " << RDF[greyValue] << endl;
				//cerr << "LUB " << LUB[greyValue] << " : " << "RUB " << RUB[greyValue] << " : " << "LDB " << LDB[greyValue] << " : " << "RDB " << RDB[greyValue] << endl;

				// trilinearly interpolate the mapped histograms (LUF, RUF, LDF, RDF & LUB, RUB, LDB, RDB)
				float p1_front = aInv * (float) LUF[greyValue] / (float)numBins + a * (float) RUF[greyValue] / (float)numBins;
				float p2_front = aInv * (float) LDF[greyValue] / (float)numBins + a * (float) RDF[greyValue] / (float)numBins;
				float q_front  = bInv * p1_front + b * p2_front;

				float p1_back = aInv * (float) LUB[greyValue] / (float)numBins + a * (float) RUB[greyValue] / (float)numBins;
				float p2_back = aInv * (float) LDB[greyValue] / (float)numBins + a * (float) RDB[greyValue] / (float)numBins;
				float q_back  = bInv * p1_back + b * p2_back;

				float ans = (cInv * q_front + c * q_back) / normFactor;
				float val = (cInv * q_front + c * q_back);
				if (val > maxVal) {
					maxVal = val;
				}

				//cerr << "Before: " << greyValue << " after " << ans << endl;

				volume[volIndex] = (uint16_t)(ans * numBins);
			}
		}
	}
	//printf("maxVal: %f, normed: %f\n", maxVal, maxVal/normFactor);
	//printf("-----\n");
}


////////////////////////////////////////////////////////////////////////////////
// 2D version of CLAHE
//
// image     - the image to perform CLAHE on 
// imageDim_ - dimensions of the given image
// numCR_    - Number of contextial regions in the _ direction (min 2, max uiMAX_REG_X/Y)
// numBins	 - Number of greyvalues for histogram (final number of grayvalues)
// cliplimit - Normalized cliplimit (higher values give more contrast)
void CLAHE::clahe2D(uint16_t* image, glm::uvec2 imageDim, glm::uvec2 numCR, unsigned int numBins, float clipLimit) {


	// calculate the size of the contextual regions
	unsigned int sizeCRx = imageDim.x / numCR.x;
	unsigned int sizeCRy = imageDim.y / numCR.y;
	unsigned long numPixelsCR = (unsigned long)sizeCRx * (unsigned long)sizeCRy;

	printf("Num Contextual Regions: %i, %i, Image size: %i, %i\n", numCR.x, numCR.y, imageDim.x, imageDim.y);
	printf("Size of Contextual Regions: %d, %d, each with %d pixels\n\n", sizeCRx, sizeCRy, numPixelsCR);
	printf("Make LUT...Image Range: [%d, %d], numBins: %d\n", _minVal, _maxVal, numBins);


	// Make the LUT - to scale the input image from NUM_GRAY_VALS to numBins
	uint16_t* LUT = new uint16_t[NUM_IN_GRAY_VALS];
	makeLUT(LUT, numBins);

	// pointer to mappings (bins)
	unsigned int* currHist;
	unsigned int* mappedHist = new unsigned int[numCR.x * numCR.y * numBins];
	memset(mappedHist, 0, sizeof(unsigned int) * numCR.x * numCR.y * numBins);

	// Calculate greylevel mappings for each contextual region 
	printf("build local histograms...\n");
	for (unsigned int currCRy = 0; currCRy < numCR.y; currCRy++) {
		unsigned int startY = currCRy * sizeCRy;
		unsigned int endY = (currCRy + 1) * sizeCRy;

		for (unsigned int currCRx = 0; currCRx < numCR.x; currCRx++) {
			unsigned int startX = currCRx * sizeCRx;
			unsigned int endX = (currCRx + 1) * sizeCRx;

			// calculate the local histogram for the given contextual region
			currHist = &mappedHist[numBins * (currCRy * numCR.x + currCRx)];
			makeHistogram2D(image, currHist, LUT, startX, startY, endX, endY, imageDim.x);

			// clip the histogram
			clipHistogram(clipLimit, numBins, currHist);

			// calculate the cumulative histogram and re-map to [min, max]
			mapHistogram(currHist, numBins, numPixelsCR);
		}
	}


	// Interpolate greylevel mappings to get CLAHE image 
	printf("interpolate...\n");

	unsigned int xRight, xLeft, yUp, yDown, subY, subX;
	unsigned int* L_Up, * L_Dn, * R_Up, * R_Dn;

	unsigned int startY = 0;
	for (unsigned int currCRy = 0; currCRy <= numCR.y; currCRy++) {

		// top row
		if (currCRy == 0) {
			subY = sizeCRy / 2;
			yUp = 0;			yDown = 0;
		}
		else {
			// bottom row
			if (currCRy == numCR.y) {
				subY = (sizeCRy + 1) / 2;
				yUp = numCR.y - 1;			yDown = yUp;
			}
			// default values
			else {
				subY = sizeCRy;
				yUp = currCRy - 1;			yDown = currCRy;
			}
		}

		unsigned int startX = 0;
		for (unsigned int currCRx = 0; currCRx <= numCR.x; currCRx++) {

			// left column
			if (currCRx == 0) {
				subX = sizeCRx / 2;
				xLeft = 0;			xRight = 0;
			}
			else {
				// special case: right column
				if (currCRx == numCR.x) {
					subX = (sizeCRx + 1) / 2;
					xLeft = numCR.x - 1;				xRight = xLeft;
				}
				// default values
				else {
					subX = sizeCRx;
					xLeft = currCRx - 1;			xRight = currCRx;
				}
			}

			L_Up = &mappedHist[numBins * (yUp * numCR.x + xLeft)];
			R_Up = &mappedHist[numBins * (yUp * numCR.x + xRight)];
			L_Dn = &mappedHist[numBins * (yDown * numCR.x + xLeft)];
			R_Dn = &mappedHist[numBins * (yDown * numCR.x + xRight)];

			lerp2D(image, L_Up, R_Up, L_Dn, R_Dn, subX, subY, startX, startY, LUT, numBins, imageDim.x);

			startX = startX + subX;
		}
		startY = startY + subY;
	}

	// clean up 
	delete[] mappedHist;
	delete[] LUT;

	std::cerr << "\n";
}


////////////////////////////////////////////////////////////////////////////////
// Public 2D CLAHE Methods

int CLAHE::CLAHE_2D(glm::uvec2 numCR, unsigned int numBins, float clipLimit) {

	printf("\n----- 2D CLAHE -----\n");

	chrono::high_resolution_clock::time_point t1 = chrono::high_resolution_clock::now();

	// error checking 
	if (numCR.x > _maxNumCR_X) return -1;		// # of x critical regions too large
	if (numCR.y > _maxNumCR_Y) return -2;		// # of y critical regions too large
	if (clipLimit == 1.0) return 0;				// return original image
	if (numBins == 0) numBins = NUM_IN_GRAY_VALS;	// default value when not specified

	// get the data from the image 
	uint16_t* imageData = new uint16_t[_imgDimX * _imgDimY];
	memcpy(imageData, _imageData, sizeof(uint16_t) * _imgDimX * _imgDimY);

	// perform CLAHE on that image data 
	clahe2D(imageData, glm::uvec2(_imgDimX, _imgDimY), numCR, numBins, clipLimit);

	// store the new data as a texture 
	GLuint textureID = initTexture2D(_imgDimX, _imgDimY, GL_R16, GL_RED, GL_UNSIGNED_SHORT, GL_LINEAR, imageData);

	chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();
	chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - t1);

	cerr << "2D CLAHE took: " << time_span.count() << " seconds.\n";

	//float max = 0.0f;
	//uint16_t* vol = imageData;
	//for (unsigned int i = 0; i < _imgDimX * _imgDimY * _imgDimZ; i++) {
	//	float value = vol[i];
	//	if (value > max) {
	//		max = value;
	//	}
	//}
	//printf("max: %f\n\n", max);
	printf("\n");
	return textureID;
}

int CLAHE::Focused_CLAHE_2D(glm::uvec2 min, glm::uvec2 max, unsigned int numBins, float clipLimit) {
	
	chrono::high_resolution_clock::time_point t1 = chrono::high_resolution_clock::now();

	unsigned int xDim = (max.x - min.x);
	unsigned int yDim = (max.y - min.y);

	// get the data from the image 
	uint16_t* imageData = new uint16_t[_imgDimX * _imgDimY];
	memcpy(imageData, _imageData, sizeof(uint16_t) * _imgDimX * _imgDimY);

	// take out the section you want to enhance
	uint16_t* subImageData = new uint16_t[ xDim * yDim ];
	memset(subImageData, 0, sizeof(uint16_t) * xDim * yDim);
	for (unsigned int xIndex = min.x, x = 0; xIndex < max.x; xIndex++, x++) {
		for (unsigned int yIndex = min.y, y = 0; yIndex < max.y; yIndex++, y++) {
			subImageData[y * xDim + x] = imageData[yIndex * _imgDimX + xIndex];
		}
	}

	// Determine the number of CR
	unsigned int numCRx = std::max((int)(xDim / 100), 1);
	unsigned int numCRy = std::max((int)(yDim / 100), 1);

	// perform CLAHE on the selected region
	printf("\n----- Focused 2D CLAHE -----\n");
	printf("x range: [%d, %d], y range: [%d, %d]\n", min.x, max.x, min.y, max.y);
	clahe2D(subImageData, glm::uvec2(xDim, yDim), glm::uvec2(numCRx, numCRy), numBins, clipLimit);

	// put the section back into the image
	for (unsigned int xIndex = min.x, x = 0; xIndex < max.x; xIndex++, x++) {
		for (unsigned int yIndex = min.y, y = 0; yIndex < max.y; yIndex++, y++) {
			imageData[yIndex * _imgDimX + xIndex] = subImageData[y * xDim + x];
		}
	}
	chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();
	chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - t1);

	cerr << "focused 2D CLAHE took: " << time_span.count() << " seconds.\n";

	// make a texture for the resulting image
	return initTexture2D(_imgDimX, _imgDimY, GL_R16, GL_RED, GL_UNSIGNED_SHORT, GL_LINEAR, imageData);
}


////////////////////////////////////////////////////////////////////////////////
// 3D version of CLAHE
//
// volDim   - Dimensions of the volume you are performing CLAHE on
// numSB    - Number of sub blocks in the x/y/z direction (min 2, max uiMAX_REG_X/Y/Z)
// numBins	- Number of greyvalues for histogram (final number of grayvalues)
// cliplimit- Normalized cliplimit (higher values give more contrast)
void CLAHE::clahe3D(uint16_t* volume, glm::uvec3 volDim, glm::uvec3 numSB, unsigned int numBins, float clipLimit) {

	// calculate the size of the contextual regions
	unsigned int sizeSBx = volDim.x / numSB.x;
	unsigned int sizeSBy = volDim.y / numSB.y;
	unsigned int sizeSBz = volDim.z / numSB.z;
	unsigned long numPixelsSB = (unsigned long)sizeSBx * (unsigned long)sizeSBy * (unsigned long)sizeSBz;

	printf("Num SubBlocks: %i, %i, %i, Volume size: %i, %i, %i\n", numSB.x, numSB.y, numSB.z, _imgDimX, _imgDimY, _imgDimZ);
	printf("Size of SubBlocks: %d, %d, %d, each with %d pixels\n\n", sizeSBx, sizeSBy, sizeSBz, numPixelsSB);
	printf("Make LUT...Volume Range: [%d, %d], numBins: %d\n", _minVal, _maxVal, numBins);

	// Make the LUT - to scale the input image from NUM_GRAY_VALS to numBins
	uint16_t* LUT = new uint16_t[NUM_IN_GRAY_VALS];
	makeLUT(LUT, numBins);

	// pointer to mappings (bins)
	unsigned int* currHist;
	unsigned int* mappedHist = new unsigned int[numSB.x * numSB.y * numSB.z * numBins];
	memset(mappedHist, 0, sizeof(unsigned int) * numSB.x * numSB.y * numSB.z * numBins);

	// Calculate greylevel mappings for each sub block 
	printf("build local histograms...\n");
	for (unsigned int currSBz = 0; currSBz < numSB.z; currSBz++) {
		unsigned int startZ = currSBz * sizeSBz;
		unsigned int endZ = (currSBz + 1) * sizeSBz;

		for (unsigned int currSBy = 0; currSBy < numSB.y; currSBy++) {
			unsigned int startY = currSBy * sizeSBy;
			unsigned int endY = (currSBy + 1) * sizeSBy;

			for (unsigned int currSBx = 0; currSBx < numSB.x; currSBx++) {
				unsigned int startX = currSBx * sizeSBx;
				unsigned int endX = (currSBx + 1) * sizeSBx;

				// get the Histogram for the given subBlock
				int histIndex = (currSBz * numSB.y * numSB.x + currSBy * numSB.x + currSBx);
				currHist = &mappedHist[numBins * (histIndex)];

				// calculate the local histogram for the given subBlock
				makeHistogram3D(volume, startX, startY, startZ, endX, endY, endZ, currHist, LUT, volDim.x, volDim.y);
				
				// clip the Histogram
				clipHistogram(clipLimit, numBins, currHist);

				// calculate the cumulative histogram and re-map to [min, max]
				mapHistogram(currHist, numBins, numPixelsSB);
			}
		}
	}

	// Interpolate greylevel mappings to get CLAHE image 
	printf("interpolate...\n");
	unsigned int xRight, xLeft, yUp, yDown, zFront, zBack;
	unsigned int sectionSizeX, sectionSizeY, sectionSizeZ;	// size of the vol to interpolate over
	// pointers to the mapped histograms to interpolate between
	unsigned int* L_Up_front, * L_Dn_front, * R_Up_front, * R_Dn_front;
	unsigned int* L_Up_back, * L_Dn_back, * R_Up_back, * R_Dn_back;

	unsigned int startZ = 0;
	for (unsigned int currSBz = 0; currSBz <= numSB.z; currSBz++) {

		// front depth
		if (currSBz == 0) {
			sectionSizeZ = sizeSBz / 2;
			zFront = 0;					zBack = 0;
		}
		else {
			// back depth
			if (currSBz == numSB.z) {
				sectionSizeZ = (sizeSBz + 1) / 2;
				zFront = numSB.z - 1;			zBack = zFront;
			}
			// default values
			else {
				sectionSizeZ = sizeSBz;
				zFront = currSBz - 1;			zBack = currSBz;
			}
		}

		unsigned int startY = 0;
		for (unsigned int currSBy = 0; currSBy <= numSB.y; currSBy++) {

			// top row
			if (currSBy == 0) {
				sectionSizeY = sizeSBy / 2;
				yUp = 0;			yDown = 0;
			}
			else {
				// bottom row
				if (currSBy == numSB.y) {
					sectionSizeY = (sizeSBy + 1) / 2;
					yUp = numSB.y - 1;			yDown = yUp;
				}
				// default values
				else {
					sectionSizeY = sizeSBy;
					yUp = currSBy - 1;			yDown = currSBy;
				}
			}

			unsigned int startX = 0;
			for (unsigned int currSBx = 0; currSBx <= numSB.x; currSBx++) {

				// left column
				if (currSBx == 0) {
					sectionSizeX = sizeSBx / 2;
					xLeft = 0;			xRight = 0;
				}
				else {
					// special case: right column
					if (currSBx == numSB.x) {
						sectionSizeX = (sizeSBx + 1) / 2;
						xLeft = numSB.x - 1;				xRight = xLeft;
					}
					// default values
					else {
						sectionSizeX = sizeSBx;
						xLeft = currSBx - 1;			xRight = currSBx;
					}
				}

				L_Up_front = &mappedHist[numBins * (zFront * numSB.y * numSB.x + yUp * numSB.x + xLeft)];
				R_Up_front = &mappedHist[numBins * (zFront * numSB.y * numSB.x + yUp * numSB.x + xRight)];
				L_Dn_front = &mappedHist[numBins * (zFront * numSB.y * numSB.x + yDown * numSB.x + xLeft)];
				R_Dn_front = &mappedHist[numBins * (zFront * numSB.y * numSB.x + yDown * numSB.x + xRight)];

				L_Up_back = &mappedHist[numBins * (zBack * numSB.y * numSB.x + yUp * numSB.x + xLeft)];
				R_Up_back = &mappedHist[numBins * (zBack * numSB.y * numSB.x + yUp * numSB.x + xRight)];
				L_Dn_back = &mappedHist[numBins * (zBack * numSB.y * numSB.x + yDown * numSB.x + xLeft)];
				R_Dn_back = &mappedHist[numBins * (zBack * numSB.y * numSB.x + yDown * numSB.x + xRight)];

				lerp3D(volume, L_Up_front, R_Up_front, L_Dn_front, R_Dn_front, L_Up_back, R_Up_back, L_Dn_back, R_Dn_back,
					sectionSizeX, sectionSizeY, sectionSizeZ, startX, startY, startZ, LUT, numBins, volDim.x, volDim.y);

				// increment the index
				startX = startX + sectionSizeX;
			}
			// increment the index
			startY = startY + sectionSizeY;
		}
		// increment the index
		startZ = startZ + sectionSizeZ;
	}

	// clean up 
	delete[] mappedHist;
	delete[] LUT;
}



////////////////////////////////////////////////////////////////////////////////
// Public 3D CLAHE Methods

int CLAHE::CLAHE_3D(glm::uvec3 numSB, unsigned int numBins, float clipLimit) {

	std::cerr << "\n----- 3D CLAHE -----\n";

	chrono::high_resolution_clock::time_point t1 = chrono::high_resolution_clock::now();

	// error checking 
	//if (clipLimit == 1.0) return 0;				// return original image
	if (numSB.x > _maxNumCR_X) return 0;				// # of x SubBlocks too large
	if (numSB.y > _maxNumCR_Y) return 0;				// # of y SubBlocks too large
	//if (numSB.z > _maxNumCR_Z) return -3;			// # of z SubBlocks too large
	if (numBins == 0) numBins = NUM_IN_GRAY_VALS;	// default value when not specified

	// get the data from the image 
	uint16_t* volumeData = new uint16_t[_imgDimX * _imgDimY * _imgDimZ];
	memcpy(volumeData, _imageData, sizeof(uint16_t) * _imgDimX * _imgDimY * _imgDimZ);

	// perform CLAHE on that volume data 
	clahe3D(volumeData, glm::uvec3(_imgDimX, _imgDimY, _imgDimZ), numSB, numBins, clipLimit);


	// store the new data as a texture 
	GLuint textureID = initTexture3D(_imgDimX, _imgDimY, _imgDimZ, GL_R16, GL_RED, GL_UNSIGNED_SHORT, GL_LINEAR, volumeData);
	//GLuint textureID = initTexture3D(_imgDimX, _imgDimY, _imgDimZ, GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT, GL_LINEAR, volumeData);

	std::cerr << "new TextureID: " << textureID << endl;

	chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();
	chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - t1);

	std::cerr << "\n3D CLAHE took: " << time_span.count() << " seconds.\n";

	return textureID;
}

int CLAHE::Focused_CLAHE_3D(glm::uvec3 min, glm::uvec3 max, unsigned int numBins, float clipLimit) {

	chrono::high_resolution_clock::time_point t1 = chrono::high_resolution_clock::now();

	unsigned int xDim = (max.x - min.x);
	unsigned int yDim = (max.y - min.y);
	unsigned int zDim = (max.z - min.z);

	// get the data from the image 
	uint16_t* volumeData = new uint16_t[_imgDimX * _imgDimY * _imgDimZ];
	memcpy(volumeData, _imageData, sizeof(uint16_t) * _imgDimX * _imgDimY * _imgDimZ);

	// take out the section you want to enhance
	uint16_t* subVolumeData = new uint16_t[xDim * yDim * zDim];
	memset(subVolumeData, 0, sizeof(uint16_t) * xDim * yDim * zDim);
	uint16_t maxVal = 0;
	uint16_t minVal = numBins;
	for (unsigned int zIndex = min.z, z = 0; zIndex < max.z; zIndex++, z++) {
		for (unsigned int yIndex = min.y, y = 0; yIndex < max.y; yIndex++, y++) {
			for (unsigned int xIndex = min.x, x = 0; xIndex < max.x; xIndex++, x++) {
				subVolumeData[z * xDim * yDim + y * xDim + x] = volumeData[zIndex * _imgDimX * _imgDimY + yIndex * _imgDimX + xIndex];
				if (volumeData[zIndex * _imgDimX * _imgDimY + yIndex * _imgDimX + xIndex] > maxVal) {
					maxVal = volumeData[zIndex * _imgDimX * _imgDimY + yIndex * _imgDimX + xIndex];
				}
				if (volumeData[zIndex * _imgDimX * _imgDimY + yIndex * _imgDimX + xIndex] < minVal) {
					minVal = volumeData[zIndex * _imgDimX * _imgDimY + yIndex * _imgDimX + xIndex];
				}
			}
		}
	}

	// Determine the number of SB
	unsigned int numSBx = std::max((int)(xDim / 100), 1);
	unsigned int numSBy = std::max((int)(yDim / 100), 1);
	unsigned int numSBz = std::max((int)(zDim / 100), 1);

	// perform CLAHE on the selected region
	printf("\n----- Focused 3D CLAHE -----\n");	
	printf("Range of values in the focused region: (%d, %d)\n", maxVal);
	printf("x range: [%d, %d], y range: [%d, %d], z range: [%d, %d]\n", min.x, max.x, min.y, max.y, min.z, max.z);
	printf("clipLimit: %f, numSubBlocks: (%u, %u, %u)\n", clipLimit, numSBx, numSBy, numSBz);
	clahe3D(subVolumeData, glm::uvec3(xDim, yDim, zDim), glm::uvec3(numSBx, numSBy, numSBz), numBins, clipLimit);

	// put the section back into the volume
	for (unsigned int zIndex = min.z, z = 0; zIndex < max.z; zIndex++, z++) {
		for (unsigned int yIndex = min.y, y = 0; yIndex < max.y; yIndex++, y++) {
			for (unsigned int xIndex = min.x, x = 0; xIndex < max.x; xIndex++, x++) {
				volumeData[zIndex * _imgDimX * _imgDimY + yIndex * _imgDimX + xIndex] = subVolumeData[z * xDim * yDim + y * xDim + x];
			}
		}
	}


	chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();
	chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - t1);

	std::cerr << "\nFocused 3D CLAHE took: " << time_span.count() << " seconds.\n";

	return initTexture3D(_imgDimX, _imgDimY, _imgDimZ, GL_R16, GL_RED, GL_UNSIGNED_SHORT, GL_LINEAR, volumeData);
	//return initTexture3D(xDim, yDim, zDim, GL_R16, GL_RED, GL_UNSIGNED_SHORT, GL_LINEAR, subVolumeData);
}


////////////////////////////////////////////////////////////////////////////////
// Printing Helper Functions

void CLAHE::printHist(unsigned int* hist, unsigned int max, unsigned int min) {

	cerr << "Histogram: \n";
	unsigned int count = 0;
	for (unsigned int i = min; i < max-1; i++) {
		if (count == 0) {
			printf("%6d: ", i/8);
		}
		//printf("(%3d, %d) ", i, hist[i]);
		printf(" %6d ", hist[i]);
		if (count == 7) {
			cerr << endl;
			count = -1;
		}
		count++;
	}
	//printf("(%3d, %d)\n", max-1, hist[max-1]);
	printf(" %6d\n", hist[max-1]);
}

void CLAHE::maxHistVal(unsigned int* hist, unsigned int max) {
	unsigned int maxVal = 0;
	for (unsigned int i = 0; i < max; i++) {
		unsigned int val = hist[i];
		if (val < maxVal) {
			maxVal = val;
		}
	}
	printf("Max value for the hist: %d\n", max);
}

void CLAHE::countHist(unsigned int* hist, unsigned int max) {
	unsigned int count = 0;
	for (unsigned int i = 0; i < max; i++) {
		count += hist[i];
	}
	printf("Hist has %d values\n", count);
}

////////////////////////////////////////////////////////////////////////////////