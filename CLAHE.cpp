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
	uint16_t min = NUM_IN_GRAY_VALS;
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
	uint16_t min = NUM_IN_GRAY_VALS;
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
	unsigned int startX, unsigned int startY, unsigned int endX, unsigned int endY) {

	// cycle through the section of the image 
	for (unsigned int currY = startY; currY < endY; currY++) {
		for (unsigned int currX = startX; currX < endX; currX++) {

			// increment the localHist for the image grayvalue 
			localHist[LUT[image[currY * _imgDimX + currX]]]++;
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
	unsigned int endX, unsigned int endY, unsigned int endZ, unsigned int* localHist, uint16_t* LUT) {

	// cycle over the subBlock of the volume 
	for (unsigned int currZ = startZ; currZ < endZ; currZ++) {
		for (unsigned int currY = startY; currY < endY; currY++) {
			for (unsigned int currX = startX; currX < endX; currX++) {

				// increment the localHist for the volume grayValue 
				localHist[LUT[volume[currZ * _imgDimX * _imgDimY + currY * _imgDimX + currX]]]++;
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
	float max = 0.0f;
	unsigned int* hist = localHist;
	for (unsigned int i = 0; i < numBins; i++) {
		float value = (float)hist[i];
		if (value > max) {
			max = value;
		}
	}
	float clipValue = (float)clipLimit * max;
	//printf("max: %f, clipValue: %f\n", max, clipValue);

	// calculate the total number of excess pixels
	unsigned long excess = 0.0;			
	hist = localHist;
	for (unsigned int i = 0; i < numBins; i++) {
		float toRemove = (float)hist[i] - clipValue;
		if (toRemove > 0.0f) {
			excess += (unsigned long)toRemove;
		}
	}
	//cerr << "1. number of excess: " << excess << endl;

	// Clip histogram and redistribute excess pixels in each bin 
	unsigned long avgInc = (float) excess / (float) numBins;
	float upper = clipValue - avgInc;	// Bins larger than upper set to clipValue
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
	//cerr << "2. number of excess: " << excess << endl;

	// Redistribute the remaining excess pixels
	while (excess > 0.0) { 
		unsigned int* endPointer = &localHist[numBins];
		unsigned int* histPointer = localHist;
		while (excess > 0.0 && histPointer < endPointer) {

			unsigned long stepSize = numBins / excess;
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
		}
	}

}


/////// MAP HISTOGRAM ///////
// calculate the equalized lookup table (mapping) by cqlculating the CDF 
// NOTE: lookup table is rescaled in range[min...max]
// Max and Min are same as for the LUT creation
void CLAHE::mapHistogram(unsigned int* localHist, unsigned int numBins, unsigned long numPixelsCR) {

	float sum = 0;
	const float scale = ((float)(_maxVal - _minVal)) / (float) numPixelsCR;

	// for each bin
	for (unsigned int i = 0; i < numBins; i++) {

		// add the histogram value for this contextual region to the sum 
		sum += localHist[i]; 

		// to normalize the cdf
		float val = (min(_minVal + sum * scale, (float)_maxVal));
		//cerr << "Val: " << val << " sum: " << sum << endl;
		localHist[i] = (unsigned int)(min(_minVal + sum * scale, (float)_maxVal));
	}
}


/////// LERP - 2D ///////
// LU, RU, LB, RB - loal mapped histograms 
// sizeX, sizeY   - size of subImage we are interpolating over
// LUT			  - mapping from image's gray values to the desired gray value range
void CLAHE::lerp2D(uint16_t* image, unsigned int* LU, unsigned int* RU, unsigned int* LB, unsigned int* RB, 
	unsigned int sizeX, unsigned int sizeY, unsigned int startX, unsigned int startY, uint16_t* LUT, unsigned int numBins) {
	
	// Normalization factor
	float normFactor = sizeX * sizeY;
	//normFactor /= ((float)numBins / (float)_maxVal);

	// for each pixel in the subImage
	for (unsigned int b = 0; b < sizeY; b++) {
		unsigned int bInv = sizeY - b;
		unsigned int currY = startY + b;

		for (unsigned int a = 0;  a < sizeX; a++) {
			unsigned int aInv = sizeX - a;
			unsigned int currX = startX + a;

			//uint16_t greyValue = LUT[*imgPointer];
			uint16_t greyValue = LUT[image[currY * _imgDimX + currX]];

			// bilinearly interpolate the mapped histograms (LU, RU, LB, RB)
			unsigned int p1 = aInv * LU[greyValue] + a * RU[greyValue];
			unsigned int p2 = aInv * LB[greyValue] + a * RB[greyValue];
			unsigned int ans = ((bInv * p1 + b * p2) / (unsigned int)normFactor);

			//*imgPointer++ = (uint16_t)ans;
			image[currY * _imgDimX + currX] = (uint16_t)ans;

			//*imgPointer++ = (uint16_t)((aInv * (bInv * LU[greyValue] + b * RU[greyValue])
			//							+ a  * (bInv * LB[greyValue] + b * RB[greyValue])) / normFactor);
		}
		//imgPointer += _imgDimX - sizeX;
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
	unsigned int sizeX, unsigned int sizeY, unsigned int sizeZ, unsigned int startX, 
	unsigned int startY, unsigned int startZ, uint16_t* LUT, unsigned int numBins) {

	// Normalization factor
	float normFactor = sizeX * sizeY * sizeZ;
	//normFactor /= ((float)numBins / (float)_maxVal);

	//printf("-----\n3D lerp\n"); 
	//printf("StartingIndex: (%d, %d, %d)\t", startX, startY, startZ);
	//printf("sizeSB: (%d, %d, %d)\n", sizeX, sizeY, sizeZ);

	// for each voxel in the subVolume
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
				int volIndex = currZ * _imgDimX * _imgDimY + currY * _imgDimX + currX;
				uint16_t greyValue = LUT[volume[volIndex]];

				//printf("volume index: %d\n", volIndex);
				//printf("index: %d, \ta: %d, b: %d, c: %d\n", index, a, b, c);
				//cerr << "LUF " << LUF[greyValue] << " : " << "RUF " << RUF[greyValue] << " : "
				//	 << "LDF " << LDF[greyValue] << " : " << "RDF " << RDF[greyValue] << endl;
				//cerr << "LUB " << LUB[greyValue] << " : " << "RUB " << RUB[greyValue] << " : "
				//	 << "LDB " << LDB[greyValue] << " : " << "RDB " << RDB[greyValue] << endl;

				// trilinearly interpolate the mapped histograms (LUF, RUF, LDF, RDF & LUB, RUB, LDB, RDB)
				unsigned int p1_front = aInv * LUF[greyValue] + a * RUF[greyValue];
				unsigned int p2_front = aInv * LDF[greyValue] + a * RDF[greyValue];
				unsigned int q_front  = bInv * p1_front + b * p2_front;

				unsigned int p1_back = aInv * LUB[greyValue] + a * RUB[greyValue];
				unsigned int p2_back = aInv * LDB[greyValue] + a * RDB[greyValue];
				unsigned int q_back  = bInv * p1_back + b * p2_back;

				unsigned int ans = ((cInv * q_front + c * q_back) / (unsigned int)normFactor);
				//cerr << "Before: " << greyValue << " after " << ans << endl;

				volume[volIndex] = (uint16_t)ans;
			}
		}
	}
	//cerr << "-----\n";
}


////////////////////////////////////////////////////////////////////////////////
// 2D version of CLAHE
//
// numCR_   - Number of contextial regions in the _ direction (min 2, max uiMAX_REG_X/Y)
// numBins	- Number of greyvalues for histogram (final number of grayvalues)
// cliplimit- Normalized cliplimit (higher values give more contrast)
int CLAHE::CLAHE_2D(unsigned int numCRx, unsigned int numCRy, unsigned int numBins, float clipLimit) {

	chrono::high_resolution_clock::time_point t1 = chrono::high_resolution_clock::now();

	// error checking 
	if (numCRx > _maxNumCR_X) return -1;		// # of x critical regions too large
	if (numCRy > _maxNumCR_Y) return -2;		// # of y critical regions too large
	if (clipLimit == 1.0) return 0;				// return original image
	if (numBins == 0) numBins = NUM_IN_GRAY_VALS;	// default value when not specified


	// calculate the size of the contextual regions
	unsigned int sizeCRx = _imgDimX / numCRx;
	unsigned int sizeCRy = _imgDimY / numCRy;
	unsigned long numPixelsCR = (unsigned long)sizeCRx * (unsigned long)sizeCRy;

	printf("\n----- 2D CLAHE -----\n");
	printf("Num Contextual Regions: %i, %i, Image size: %i, %i\n", numCRx, numCRy, _imgDimX, _imgDimY);
	printf("Size of Contextual Regions: %d, %d, each with %d pixels\n\n", sizeCRx, sizeCRy, numPixelsCR);
	printf("Make LUT...Image Range: [%d, %d], numBins: %d\n", _minVal, _maxVal, numBins);

	// Make the LUT - to scale the input image from NUM_GRAY_VALS to numBins
	uint16_t* LUT = new uint16_t[NUM_IN_GRAY_VALS];
	makeLUT(LUT, numBins);

	// pointer to mappings (bins)
	unsigned int* currHist;
	unsigned int* mappedHist = new unsigned int[numCRx * numCRy * numBins];
	memset(mappedHist, 0, sizeof(unsigned int) * numCRx * numCRy * numBins);

	// Calculate greylevel mappings for each contextual region 
	printf("build local histograms...\n");
	for (unsigned int currCRy = 0; currCRy < numCRy; currCRy++) {
		unsigned int startY = currCRy * sizeCRy;
		unsigned int endY = (currCRy + 1) * sizeCRy;

		for (unsigned int currCRx = 0; currCRx < numCRx; currCRx++) {
			unsigned int startX = currCRx * sizeCRx;
			unsigned int endX = (currCRx + 1) * sizeCRx;

			// calculate the local histogram for the given contextual region
			currHist = &mappedHist[numBins * (currCRy * numCRx + currCRx)];
			makeHistogram2D(_imageData, currHist, LUT, startX, startY, endX, endY);

			// clip the histogram
			clipHistogram(clipLimit, numBins, currHist);

			// calculate the cumulative histogram and re-map to [min, max]
			mapHistogram(currHist, numBins, numPixelsCR);
		}
	}

	
	// Interpolate greylevel mappings to get CLAHE image 
	printf("interpolate...\n");

	unsigned int xRight, xLeft, yUp, yDown, subY, subX;
	unsigned int *L_Up, *L_Dn, *R_Up, *R_Dn; 

	unsigned int startY = 0;
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

		unsigned int startX = 0;
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

			//cerr << "L_up: " << numBins * (yUp * numCRx + xLeft) << endl;
			//cerr << "R_up: " << numBins * (yUp * numCRx + xRight) << endl;
			//cerr << "L_dn: " << numBins * (yDown * numCRx + xLeft) << endl;
			//cerr << "R_dn: " << numBins * (yDown * numCRx + xRight) << endl;
			//cerr << "-----\n";

			lerp2D(_imageData, L_Up, R_Up, L_Dn, R_Dn, subX, subY, startX, startY, LUT, numBins);

			startX = startX + subX;
		}
		startY = startY + subY;
	}

	// clean up 
	delete[] mappedHist;
	delete[] LUT;

	cerr << "fin\n\n";

	// store the new data as a texture 
	GLuint textureID = initTexture2D(_imgDimX, _imgDimY, GL_R16, GL_RED, GL_UNSIGNED_SHORT, GL_LINEAR, _imageData);

	chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();
	chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - t1);

	cerr << "2D CLAHE took: " << time_span.count() << " seconds.\n";

	float max = 0.0f;
	uint16_t* vol = _imageData;
	for (unsigned int i = 0; i < _imgDimX * _imgDimY * _imgDimZ; i++) {
		float value = vol[i];
		if (value > max) {
			max = value;
		}
	}
	printf("max: %f\n\n", max);
	return textureID;
}


////////////////////////////////////////////////////////////////////////////////
// 3D version of CLAHE
//
// numCR_   - Number of contextial regions in the _ direction (min 2, max uiMAX_REG_X/Y/Z)
// numBins	- Number of greyvalues for histogram (final number of grayvalues)
// cliplimit- Normalized cliplimit (higher values give more contrast)
int CLAHE::CLAHE_3D(unsigned int numSBx, unsigned int numSBy, unsigned int numSBz, unsigned int numBins, float clipLimit) {

	chrono::high_resolution_clock::time_point t1 = chrono::high_resolution_clock::now();

	// error checking 
	if (clipLimit == 1.0) return 0;					// return original image
	if (numSBx > _maxNumCR_X) return -1;			// # of x SubBlocks too large
	if (numSBy > _maxNumCR_Y) return -2;			// # of y SubBlocks too large
	//if (numSBz > _maxNumCR_Z) return -3;			// # of z SubBlocks too large
	if (numBins == 0) numBins = NUM_IN_GRAY_VALS;	// default value when not specified


	// calculate the size of the contextual regions
	unsigned int sizeSBx = _imgDimX / numSBx;
	unsigned int sizeSBy = _imgDimY / numSBy;
	unsigned int sizeSBz = _imgDimZ / numSBz;
	unsigned long numPixelsSB = (unsigned long)sizeSBx * (unsigned long)sizeSBy * (unsigned long)sizeSBz;

	cerr << "\n----- 3D CLAHE -----\n";
	printf("Num SubBlocks: %i, %i, %i, Volume size: %i, %i, %i\n", numSBx, numSBy, numSBz, _imgDimX, _imgDimY, _imgDimZ);
	printf("Size of SubBlocks: %d, %d, %d, each with %d pixels\n\n", sizeSBx, sizeSBy, sizeSBz, numPixelsSB);
	printf("Make LUT...Volume Range: [%d, %d], numBins: %d\n", _minVal, _maxVal, numBins);

	// Make the LUT - to scale the input image from NUM_GRAY_VALS to numBins
	uint16_t* LUT = new uint16_t[NUM_IN_GRAY_VALS];
	makeLUT(LUT, numBins);


	// pointer to mappings (bins)
	unsigned int* currHist;
	unsigned int* mappedHist = new unsigned int[numSBx * numSBy * numSBz * numBins];
	memset(mappedHist, 0, sizeof(unsigned int) * numSBx * numSBy * numSBz * numBins);

	// Calculate greylevel mappings for each contextual region 
	printf("build local histograms...\n");
	for (unsigned int currSBz = 0; currSBz < numSBz; currSBz++) {
		unsigned int startZ = currSBz * sizeSBz;
		unsigned int endZ = (currSBz + 1) * sizeSBz;

		for (unsigned int currSBy = 0; currSBy < numSBy; currSBy++) {
			unsigned int startY = currSBy * sizeSBy;
			unsigned int endY = (currSBy + 1) * sizeSBy;

			for (unsigned int currSBx = 0; currSBx < numSBx; currSBx++) {
				unsigned int startX = currSBx * sizeSBx;
				unsigned int endX = (currSBx + 1) * sizeSBx;

				// get the Histogram for the given subBlock
				int histIndex = (currSBz * numSBy * numSBx + currSBy * numSBx + currSBx);
				currHist = &mappedHist[numBins * (histIndex)];

				// calculate the local histogram for the given subBlock
				makeHistogram3D(_imageData, startX, startY, startZ, endX, endY, endZ, currHist, LUT);

				//printf("SB: (%d, %d, %d), \thistIndex: %d\n", currSBx, currSBy, currSBz, histIndex);
				//printHist(currHist, numBins);

				// clip the Histogram
				//clipHistogram(clipLimit, numBins, currHist);

				// calculate the cumulative histogram and re-map to [min, max]
				mapHistogram(currHist, numBins, numPixelsSB);
				//printHist(currHist, numBins);
			}
		}
	}


	// Interpolate greylevel mappings to get CLAHE image 
	printf("interpolate...\n");
	unsigned int xRight, xLeft, yUp, yDown, zFront, zBack;
	unsigned int subY, subX, subZ;	// size of the vol to interpolate over
	// pointers to the mapped histograms to interpolate between
	unsigned int *L_Up_front, *L_Dn_front, *R_Up_front, *R_Dn_front;
	unsigned int *L_Up_back,  *L_Dn_back,  *R_Up_back,  *R_Dn_back;

	unsigned int startZ = 0;
	for (unsigned int currSBz = 0; currSBz <= numSBz; currSBz++) {

		// front depth
		if (currSBz == 0) {
			subZ = sizeSBz / 2;
			zFront = 0;					zBack = 0;
		}
		else {	
			// back depth
			if (currSBz == numSBz) {
				subZ = int((sizeSBz + 1) / 2);
				zFront = numSBz - 1;			zBack = zFront;
			}
			// default values
			else {
				subZ = sizeSBz;
				zFront = currSBz - 1;			zBack = currSBz;
			}
		}

		unsigned int startY = 0;
		for (unsigned int currSBy = 0; currSBy <= numSBy; currSBy++) {

			// top row
			if (currSBy == 0) {
				subY = sizeSBy / 2;
				yUp = 0;			yDown = 0;
			}
			else {
				// bottom row
				if (currSBy == numSBy) {
					subY = (sizeSBy + 1) / 2;
					yUp = numSBy - 1;			yDown = yUp;
				}
				// default values
				else {
					subY = sizeSBy;
					yUp = currSBy - 1;			yDown = currSBy;
				}
			}

			unsigned int startX = 0;
			for (unsigned int currSBx = 0; currSBx <= numSBx; currSBx++) {

				// left column
				if (currSBx == 0) {
					subX = sizeSBx / 2;
					xLeft = 0;			xRight = 0;
				}
				else {
					// special case: right column
					if (currSBx == numSBx) {
						subX = (sizeSBx + 1) / 2;
						xLeft = numSBx - 1;				xRight = xLeft;
					}
					// default values
					else {
						subX = sizeSBx;
						xLeft = currSBx - 1;			xRight = currSBx;
					}
				}

				//			(currSBz * numSBy * numSBx + currSBy * numSBx + currSBx)
				//cerr << "----------------------------------------------------------------------\n";
				//cerr << "index: " << "currSB: " << currSBx << ", " << currSBy << ", " << currSBz << "\n";
				//cerr << "LUF " << zFront * numSBy * numSBx + yUp   * numSBx + xLeft << " : " << "RUF " << zFront * numSBy * numSBx + yUp   * numSBx + xRight << " : "
				//	 << "LDF " << zFront * numSBy * numSBx + yDown * numSBx + xLeft << " : " << "RDF " << zFront * numSBy * numSBx + yDown * numSBx + xRight<< endl;
				//cerr << "LUB " << zBack  * numSBy * numSBx + yUp   * numSBx + xLeft << " : " << "RUB " << zBack  * numSBy * numSBx + yUp   * numSBx + xRight<< " : "
				//	 << "LDB " << zBack  * numSBy * numSBx + yDown * numSBx + xLeft << " : " << "RDB " << zBack  * numSBy * numSBx + yDown * numSBx + xRight << endl;

				L_Up_front = &mappedHist[numBins * (zFront * numSBy * numSBx + yUp   * numSBx + xLeft )];
				R_Up_front = &mappedHist[numBins * (zFront * numSBy * numSBx + yUp   * numSBx + xRight)];
				L_Dn_front = &mappedHist[numBins * (zFront * numSBy * numSBx + yDown * numSBx + xLeft )];
				R_Dn_front = &mappedHist[numBins * (zFront * numSBy * numSBx + yDown * numSBx + xRight)];

				L_Up_back = &mappedHist[numBins * (zBack * numSBy * numSBx + yUp * numSBx + xLeft)];
				R_Up_back = &mappedHist[numBins * (zBack * numSBy * numSBx + yUp * numSBx + xRight)];
				L_Dn_back = &mappedHist[numBins * (zBack * numSBy * numSBx + yDown * numSBx + xLeft)];
				R_Dn_back = &mappedHist[numBins * (zBack * numSBy * numSBx + yDown * numSBx + xRight)];

				lerp3D(_imageData, L_Up_front, R_Up_front, L_Dn_front, R_Dn_front, L_Up_back, R_Up_back, L_Dn_back, R_Dn_back, 
						subX, subY, subZ, startX, startY, startZ, LUT, numBins);

				// increment the index
				startX = startX + subX;
			}
			// increment the index
			startY = startY + subY;
		}
		// increment the index
		startZ = startZ + subZ;
	}
	
	// clean up 
	delete[] mappedHist;
	delete[] LUT;
	
	cerr << "fin\n";

	// re-distribute the pixels to the full range - for some reason values are being mapped to [0,4519] instead of [0,65535]... 
	float max = 0.0f;
	uint16_t* vol = _imageData;
	for (unsigned int i = 0; i < _imgDimX * _imgDimY * _imgDimZ; i++) {
		float value = vol[i];
		if (value > max) {
			max = value;
		}
	}
	printf("max: %f\n\n", max);

	vol = _imageData;
	for (unsigned int i = 0; i < _imgDimX * _imgDimY * _imgDimZ; i++) {
		float value = vol[i];
		value = (value / max) * (float)_maxVal;
		vol[i] = value;
	}

	// store the new data as a texture 
	GLuint textureID = initTexture3D(_imgDimX, _imgDimY, _imgDimZ, GL_R16, GL_RED, GL_UNSIGNED_SHORT, GL_LINEAR, _imageData);
	cerr << "new TextureID: " << textureID << endl;

	chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();
	chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - t1);

	cerr << "3D CLAHE took: " << time_span.count() << " seconds.\n";

	return textureID;
}

////////////////////////////////////////////////////////////////////////////////
// Printing Helper Functions

void CLAHE::printHist(unsigned int* hist, unsigned int max) {

	cerr << "Histogram: \n";
	unsigned int count = 0;
	for (unsigned int i = 0; i < max-1; i++) {
		printf("(%3d, %d) ", i, hist[i]);
		if (count == 7) {
			cerr << endl;
			count = -1;
		}
		count++;
	}
	printf("(%3d, %d)\n", max-1, hist[max-1]);
}

void CLAHE::printHist(float* hist, unsigned int max) {

	cerr << "Histogram: \n";
	unsigned int count = 0;
	for (unsigned int i = 0; i < max - 1; i++) {
		printf("(%3d, %d) ", i, hist[i]);
		if (count == 7) {
			cerr << endl;
			count = -1;
		}
		count++;
	}
	printf("(%3d, %d)\n", max - 1, hist[max - 1]);
}

////////////////////////////////////////////////////////////////////////////////