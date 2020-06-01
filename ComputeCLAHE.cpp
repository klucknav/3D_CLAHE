////////////////////////////////////////
// ComputeCLAHE.cpp
////////////////////////////////////////

#include "ComputeCLAHE.h"
#include "Shader.h"

#include <thread>
#include <algorithm>
#include <chrono>

using namespace std;

void mapHistogram(uint32_t minVal, uint32_t maxVal, uint32_t numPixelsSB, uint32_t numBins, uint32_t* localHist);

////////////////////////////////////////////////////////////////////////////////
// Constructors/Destructors

ComputeCLAHE::ComputeCLAHE(GLuint volumeTexture, glm::ivec3 volDims, unsigned int finalGrayVals, unsigned int inGrayVals) {
	
	// Load Shaders 
	_minMaxShader = LoadComputeShader("minMax.comp");
	_minMaxMaskedShader = LoadComputeShader("minMax_masked.comp");
	_LUTshader = LoadComputeShader("LUT.comp");
	_LUTMaskedshader = LoadComputeShader("LUT_masked.comp");

	_histShader = LoadComputeShader("hist.comp");
	_histMaskedShader = LoadComputeShader("hist_masked.comp");

	_excessShader = LoadComputeShader("excess.comp");
	_clipShaderPass1 = LoadComputeShader("clipHist.comp");
	_clipShaderPass2 = LoadComputeShader("clipHist_p2.comp");

	_lerpShader = LoadComputeShader("lerp.comp");
	_lerpFocusedShader = LoadComputeShader("lerp_focused.comp");
	_lerpMaskedShader = LoadComputeShader("lerp_masked.comp");

	// Volume Data 
	_volumeTexture = volumeTexture;
	_volDims = volDims;
	_numOutGrayVals = finalGrayVals;
	_numInGrayVals = inGrayVals;
	
	// if the number of gray values is changing --> use the LUT
	_useLUT = (_numOutGrayVals != _numInGrayVals);
	printf("useLUT: %s\n", _useLUT ? "true" : "false");
}

void ComputeCLAHE::Init(GLuint volumeTexture, GLuint maskTexture, glm::ivec3 volDims, unsigned int finalGrayVals, unsigned int inGrayVals) {
	
	// Load Shaders
	_minMaxShader = LoadComputeShader("minMax.comp");
	_minMaxMaskedShader = LoadComputeShader("minMax_masked.comp");
	_LUTshader = LoadComputeShader("LUT.comp");
	_LUTMaskedshader = LoadComputeShader("LUT_masked.comp");

	_histShader = LoadComputeShader("hist.comp");
	_histMaskedShader = LoadComputeShader("hist_masked.comp");

	_excessShader = LoadComputeShader("excess.comp");
	_clipShaderPass1 = LoadComputeShader("clipHist.comp");
	_clipShaderPass2 = LoadComputeShader("clipHist_p2.comp");

	_lerpShader = LoadComputeShader("lerp.comp");
	_lerpFocusedShader = LoadComputeShader("lerp_focused.comp");
	_lerpMaskedShader = LoadComputeShader("lerp_masked.comp");

	// Volume Data 
	_volumeTexture = volumeTexture;
	_volDims = volDims;
	_numOutGrayVals = finalGrayVals;
	_numInGrayVals = inGrayVals;
	_maskTexture = maskTexture;

	// if the number of gray values is changing --> use the LUT
	_useLUT = (_numOutGrayVals != _numInGrayVals);
	printf("useLUT: %s\n", _useLUT ? "true" : "false");
}

ComputeCLAHE::~ComputeCLAHE() {
	glDeleteProgram(_minMaxShader);
	glDeleteProgram(_minMaxMaskedShader);
	glDeleteProgram(_LUTshader);
	glDeleteProgram(_LUTMaskedshader);

	glDeleteProgram(_histShader);
	glDeleteProgram(_histMaskedShader);

	glDeleteProgram(_excessShader);
	glDeleteProgram(_clipShaderPass1);
	glDeleteProgram(_clipShaderPass2);

	glDeleteProgram(_lerpShader);
	glDeleteProgram(_lerpFocusedShader);
	glDeleteProgram(_lerpMaskedShader);
}

////////////////////////////////////////////////////////////////////////////////
// CLAHE Functions

GLuint ComputeCLAHE::Compute3D_CLAHE(GLuint& maskedVersion, glm::uvec3 numSB, float clipLimit, bool useMask) {

	printf("\n----- Compute 3D CLAHE -----\n");
	printf("using (%u, %u, %u) subBlocks\n", numSB.x, numSB.y, numSB.z);

	chrono::high_resolution_clock::time_point t1 = chrono::high_resolution_clock::now();

	// make sure the clip limit is valid	
	clipLimit = glm::clamp(clipLimit, _minClipLimit, _maxClipLimit);
	printf("clipLimit: %.2f, numSubBlocks(%u, %u, %u)\n", clipLimit, numSB.x, numSB.y, numSB.z);

	// Create the LUT
	uint32_t minMax[2] = { _numInGrayVals, 0 };
	uint32_t numPixels = computeLUT(_volDims, minMax, useMask);
	printf("Min/max: (%d, %d)\n", minMax[0], minMax[1]);

	// Create the Histogram
	if (useMask) {
		numSB = glm::uvec3(1, 1, 1);
		computeHist(_volDims, numSB, useMask);
		computeClipHist(_volDims, numSB, clipLimit, minMax, numPixels);
	}
	else {
		computeHist(_volDims, numSB, useMask);
		computeClipHist(_volDims, numSB, clipLimit, minMax);
	}

	// Interpolate to re-create the new texture
	GLuint newTexture = computeLerp(maskedVersion, _volDims, numSB, useMask);

	chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();
	chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - t1);
	printf("3D CLAHE took: %f seconds\n\n", time_span.count());

	return newTexture;
}

GLuint ComputeCLAHE::ComputeFocused3D_CLAHE(glm::ivec3 min, glm::ivec3 max, float clipLimit) {

	printf("\n----- Compute focused 3D CLAHE -----\n");

	chrono::high_resolution_clock::time_point t1 = chrono::high_resolution_clock::now();

	// Make sure the Max/Min values are valid 
	min.x = std::max(min.x, 0);
	min.y = std::max(min.y, 0);
	min.z = std::max(min.z, 0);

	max.x = std::min(max.x, _volDims.x);
	max.y = std::min(max.y, _volDims.y);
	max.z = std::min(max.z, _volDims.z);

	printf("x range: [%d, %d], y range: [%d, %d], z range: [%d, %d]\n", min.x, max.x, min.y, max.y, min.z, max.z);

	// Determine the number of SB
	glm::ivec3 focusedDim = max - min;
	if (focusedDim.x < _minPixels.x || focusedDim.y < _minPixels.y || focusedDim.z < _minPixels.z) {
		printf("Focused Region too Small\n");
		return 0;
	}
	unsigned int numSBx = std::max((focusedDim.x / _pixelRatio.x), 1);
	unsigned int numSBy = std::max((focusedDim.y / _pixelRatio.y), 1);
	unsigned int numSBz = std::max((focusedDim.z / _pixelRatio.z), 1);
	glm::uvec3 numSB = glm::uvec3(numSBx, numSBy, numSBz);

	// make sure the clip limit is valid
	if (clipLimit < _minClipLimit || clipLimit > _maxClipLimit) {
		printf("ClipLimit out of bounds (%.3f)\n", clipLimit);
		return 0;
	}
	clipLimit = std::max(std::min(clipLimit, _maxClipLimit), _minClipLimit);
	printf("clipLimit: %.2f, numSubBlocks(%u, %u, %u)\n", clipLimit, numSB.x, numSB.y, numSB.z);

	// Create the LUT
	bool useMask = false;
	_useLUT = true;
	uint32_t minMax[2] = { _numInGrayVals, 0 };
	computeLUT(focusedDim, minMax, useMask, min);
	printf("Min/max: (%d, %d)\n", minMax[0], minMax[1]);

	// Creat the Histograms
	computeHist(focusedDim, numSB, useMask, min);
	computeClipHist(focusedDim, numSB, clipLimit, minMax);

	// Interpolate to re-create the new texture
	GLuint newTexture = computeFocusedLerp(focusedDim, numSB, min, max);

	chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();
	chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - t1);
	printf("Focused 3D CLAHE took: %f seconds\n\n", time_span.count());

	return newTexture;
}

GLuint ComputeCLAHE::ComputeMasked3D_CLAHE(float clipLimit) {	
	
	printf("\n----- Compute Masked 3D CLAHE -----\n");
	bool useMask = true;

	// make sure the clip limit is valid	
	clipLimit = glm::clamp(clipLimit, _minClipLimit, _maxClipLimit);

	// Create the LUT
	uint32_t* minData = new uint32_t[_numOrgans];		std::fill_n(minData, _numOrgans, _numInGrayVals);
	uint32_t* maxData = new uint32_t[_numOrgans];		memset(maxData, 0, _numOrgans * sizeof(uint32_t));
	uint32_t* numPixels = new uint32_t[_numOrgans];		memset(numPixels, 0, _numOrgans * sizeof(uint32_t));
	computeMaskedLUT(_volDims, minData, maxData, numPixels);
	fprintf(stderr, "min: %d, max: %d\n", minData[0], maxData[0]);

	// Create and clip the Histograms 
	glm::uvec3 numSB = glm::uvec3(1, 1, 1);
	computeMaskedHist(_volDims);
	computeMaskedClipHist(_volDims, clipLimit, minData, maxData, numPixels);

	//// Create the LUT
	//_useLUT = true;
	//uint32_t minMax[2] = { _numInGrayVals, 0 };
	//uint32_t numPixels = computeLUT(_volDims, minMax, useMask);
	//printf("Min/max: (%d, %d)\n", minMax[0], minMax[1]);

	//// Create and clip the Histograms
	//glm::uvec3 numSB = glm::uvec3(1, 1, 1);
	//computeMaskedHist(_volDims);
	//computeClipHist(_volDims, numSB, clipLimit, minMax, numPixels, true);

	// Interpolate to re-create the new texture
	GLuint newTexture = computeMaskedLerp(_volDims);
	return newTexture;
}

////////////////////////////////////////////////////////////////////////////////
// CLAHE Compute Shader Functions

uint32_t ComputeCLAHE::computeLUT(glm::uvec3 volDims, uint32_t* minMax, bool useMask, glm::uvec3 offset) {

	printf("LUT...");

	////////////////////////////////////////////////////////////////////////////
	// Calculate the Min/Max Values for the volume 

	// buffer to store the min/max
	GLuint globalMinMaxBuffer;
	glGenBuffers(1, &globalMinMaxBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, globalMinMaxBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 2 * sizeof(uint32_t), minMax, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// buffer to count the number of pixels that are not masked
	GLuint unMaskedPixelBuffer;
	uint32_t numUnMaskedPixels = 0;
	glGenBuffers(1, &unMaskedPixelBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, unMaskedPixelBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(uint32_t), &numUnMaskedPixels, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Set up Compute Shader 
	glUseProgram(_minMaxShader);
	glBindImageTexture(0, _volumeTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R16UI);
	glBindImageTexture(1, _maskTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R8UI);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, globalMinMaxBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, unMaskedPixelBuffer);
	glUniform3ui(glGetUniformLocation(_minMaxShader, "offset"), offset.x, offset.y, offset.z);
	glUniform3ui(glGetUniformLocation(_minMaxShader, "volumeDims"), volDims.x, volDims.y, volDims.z);
	glUniform1i(glGetUniformLocation(_minMaxShader, "useMask"), useMask);

	glDispatchCompute(	(GLuint)((volDims.x + 3) / 4),
						(GLuint)((volDims.y + 3) / 4),
						(GLuint)((volDims.z + 3) / 4));
	// make sure writting to the image is finished before reading 
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glUseProgram(0);


	// Store the calculated global Min and Max data
	uint32_t* data = new uint32_t[2];
	memset(data, 0, 2 * sizeof(uint32_t));
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, globalMinMaxBuffer);
	data = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);

	minMax[0] = data[0], minMax[1] = data[1];

	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

	// Get the num Pixels in the masked region
	uint32_t* pixelCount = new uint32_t;
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, unMaskedPixelBuffer);
	pixelCount = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
	fprintf(stderr, "\n\nTESTING: %d \n\n", *pixelCount);
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);


	////////////////////////////////////////////////////////////////////////////
	// Compute the LUT

	// buffer to store the LUT
	glGenBuffers(1, &_LUTbuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _LUTbuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, _numInGrayVals * sizeof(uint32_t), nullptr, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	if (_useLUT) {
		// Set up Compute Shader 
		glUseProgram(_LUTshader);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, globalMinMaxBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _LUTbuffer);
		glUniform1ui(glGetUniformLocation(_LUTshader, "NUM_OUT_BINS"), _numOutGrayVals);

		glDispatchCompute((GLuint)(_numInGrayVals / 64), 1, 1);

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		glUseProgram(0);
	}
	// clean up
	glDeleteBuffers(1, &globalMinMaxBuffer);
	return *pixelCount;
}

void ComputeCLAHE::computeMaskedLUT(glm::uvec3 volDims, uint32_t* minData, uint32_t* maxData, uint32_t* pixelCount) {

	printf("Masked LUT...");

	////////////////////////////////////////////////////////////////////////////
	// Calculate the Min/Max Values for the volume 

	// buffer to store the min/max
	GLuint globalMinBuffer;
	uint32_t* tempMinData = new uint32_t[_numOrgans];
	std::fill_n(tempMinData, _numOrgans, _numInGrayVals);
	fprintf(stderr, "\ntempMin: %d, %d\n", tempMinData[0], tempMinData[0]);
	glGenBuffers(1, &globalMinBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, globalMinBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, _numOrgans * sizeof(uint32_t), tempMinData, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	GLuint globalMaxBuffer;
	uint32_t* tempMaxData = new uint32_t[_numOrgans];	
	memset(tempMaxData, 0, _numOrgans * sizeof(uint32_t));
	fprintf(stderr, "tempMax: %d, %d\n", tempMaxData[0], tempMaxData[0]);
	glGenBuffers(1, &globalMaxBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, globalMaxBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, _numOrgans * sizeof(uint32_t), tempMaxData, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// buffer to count the number of pixels that are not masked
	GLuint unMaskedPixelBuffer;
	uint32_t* tempPixelCount = new uint32_t[_numOrgans];
	memset(tempPixelCount, 0, _numOrgans * sizeof(uint32_t));
	glGenBuffers(1, &unMaskedPixelBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, unMaskedPixelBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, _numOrgans * sizeof(uint32_t), tempPixelCount, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Set up Compute Shader 
	glUseProgram(_minMaxMaskedShader);
	glBindImageTexture(0, _volumeTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R16UI);
	glBindImageTexture(1, _maskTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R8UI);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, globalMinBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, globalMaxBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, unMaskedPixelBuffer);
	glUniform3ui(glGetUniformLocation(_minMaxMaskedShader, "volumeDims"), volDims.x, volDims.y, volDims.z);

	glDispatchCompute(	(GLuint)((volDims.x + 3) / 4),
						(GLuint)((volDims.y + 3) / 4),
						(GLuint)((volDims.z + 3) / 4));
	// make sure writting to the image is finished before reading 
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glUseProgram(0);


	// Store the calculated global Min and Max data
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, globalMinBuffer);
	tempMinData = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, globalMaxBuffer);
	tempMaxData = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

	for (int i = 0; i < _numOrgans; i++) {
		minData[i] = tempMinData[i];
		maxData[i] = tempMaxData[i];
		fprintf(stderr, "\n(1)min: %d, max: %d\n", minData[i], maxData[i]);
	}

	// Get the num Pixels in the masked region
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, unMaskedPixelBuffer);
	tempPixelCount = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

	for (int i = 0; i < _numOrgans; i++) {
		pixelCount[i] = tempPixelCount[i];
	}
	fprintf(stderr, "\nPixelCount: %d, %d\n\n", pixelCount[0], pixelCount[1]);

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);


	////////////////////////////////////////////////////////////////////////////
	// Compute the LUT

	// buffer to store the LUT
	glGenBuffers(1, &_LUTbuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _LUTbuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, _numInGrayVals * _numOrgans * sizeof(uint32_t), nullptr, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Set up Compute Shader 
	glUseProgram(_LUTshader);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, globalMinBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, globalMaxBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _LUTbuffer);
	glUniform1ui(glGetUniformLocation(_LUTshader, "NUM_IN_BINS"), _numInGrayVals);
	glUniform1ui(glGetUniformLocation(_LUTshader, "NUM_OUT_BINS"), _numOutGrayVals);

	glDispatchCompute((GLuint)(_numInGrayVals / 64), 1, 1);

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	glUseProgram(0);

	// clean up
	glDeleteBuffers(1, &globalMinBuffer);
	glDeleteBuffers(1, &globalMaxBuffer);
}


void ComputeCLAHE::computeHist(glm::uvec3 volDims, glm::uvec3 numSB, bool useMask, glm::uvec3 offset) {

	printf("\nCompute Hist... (%d, %d, %d) ", numSB.x, numSB.y, numSB.z);

	// Buffer to store the Histograms
	uint32_t histSize = _numOutGrayVals * numSB.x * numSB.y * numSB.z;
	glGenBuffers(1, &_histBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _histBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, histSize * sizeof(uint32_t), nullptr, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);


	glGenBuffers(1, &_histMaxBuffer);
	uint32_t maxValSize = numSB.x * numSB.y * numSB.z;
	uint32_t* _histMax = new uint32_t[maxValSize];
	memset(_histMax, 0, maxValSize * sizeof(uint32_t));

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _histMaxBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, maxValSize * sizeof(uint32_t), _histMax, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Set up Compute Shader 
	glUseProgram(_histShader);
	glBindImageTexture(0, _volumeTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R16UI);
	glBindImageTexture(1, _maskTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R8UI);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _LUTbuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _histBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _histMaxBuffer);
	glUniform3i(glGetUniformLocation(_histShader, "numSB"), numSB.x, numSB.y, numSB.z);
	glUniform1ui(glGetUniformLocation(_histShader, "NUM_OUT_BINS"), _numOutGrayVals);
	glUniform3ui(glGetUniformLocation(_histShader, "offset"), offset.x, offset.y, offset.z);
	glUniform1i(glGetUniformLocation(_histShader, "useLUT"), _useLUT);
	glUniform3ui(glGetUniformLocation(_histShader, "volumeDims"), volDims.x, volDims.y, volDims.z);
	glUniform1i(glGetUniformLocation(_histShader, "useMask"), useMask);

	glDispatchCompute(	(GLuint)((volDims.x + 3) / 4),
						(GLuint)((volDims.y + 3) / 4),
						(GLuint)((volDims.z + 3) / 4));

	// make sure writting to the image is finished before reading 
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glUseProgram(0);
}

void ComputeCLAHE::computeMaskedHist(glm::uvec3 volDims) {

	printf("\nCompute Masked Hist... (%d organs) ", _numOrgans);

	// Buffer to store the Histograms
	uint32_t histSize = _numOutGrayVals * _numOrgans;
	glGenBuffers(1, &_histBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _histBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, histSize * sizeof(uint32_t), nullptr, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Buffer to store the max values of the histograms
	glGenBuffers(1, &_histMaxBuffer);
	uint32_t maxValSize = _numOrgans;
	uint32_t* _histMax = new uint32_t[maxValSize];
	memset(_histMax, 0, maxValSize * sizeof(uint32_t));

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _histMaxBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, maxValSize * sizeof(uint32_t), _histMax, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Set up Compute Shader 
	glUseProgram(_histMaskedShader);
	glBindImageTexture(0, _volumeTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R16UI);
	glBindImageTexture(1, _maskTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R8UI);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _LUTbuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _histBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _histMaxBuffer);
	glUniform1ui(glGetUniformLocation(_histMaskedShader, "NUM_BINS"), _numInGrayVals);
	glUniform1i(glGetUniformLocation(_histMaskedShader, "useLUT"), _useLUT);
	glUniform3ui(glGetUniformLocation(_histMaskedShader, "volumeDims"), volDims.x, volDims.y, volDims.z);

	glDispatchCompute(	(GLuint)((volDims.x + 3) / 4),
						(GLuint)((volDims.y + 3) / 4),
						(GLuint)((volDims.z + 3) / 4));

	// make sure writting to the image is finished before reading 
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glUseProgram(0);
}


void ComputeCLAHE::computeClipHist(glm::uvec3 volDims, glm::uvec3 numSB, float clipLimit, uint32_t* minMax, int numPixels, bool useMask) {

	uint32_t histSize = _numOutGrayVals * numSB.x * numSB.y * numSB.z;
	uint32_t numHistograms = numSB.x * numSB.y * numSB.z;
	if (useMask) {
		numHistograms = _numOrgans;
	}

	if (clipLimit < 1.0f) {
		std::cerr << "\nexcess ... ";
		////////////////////////////////////////////////////////////////////////////
		// Calculate the excess pixels based on the clipLimit


		// buffer for the pixels to re-distribute
		uint32_t* excess = new uint32_t[numHistograms];
		memset(excess, 0, numHistograms * sizeof(uint32_t));

		GLuint excessBuffer;
		glGenBuffers(1, &excessBuffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, excessBuffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, numHistograms * sizeof(uint32_t), excess, GL_STREAM_READ);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// Set up Compute Shader 
		glUseProgram(_excessShader);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _histBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _histMaxBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, excessBuffer);
		glUniform1ui(glGetUniformLocation(_excessShader, "NUM_BINS"), _numOutGrayVals);
		glUniform1f(glGetUniformLocation(_excessShader, "clipLimit"), clipLimit);

		glDispatchCompute((GLuint)((histSize + 63) / 64), 1, 1);

		// make sure writting to the image is finished before reading 
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		glUseProgram(0);
		delete[] excess;

		////////////////////////////////////////////////////////////////////////////
		// Clip the Histogram - Pass 1 
		// - clip the values and re-distribute some to all pixels

		std::cerr << "clip Hist 1... ";

		glUseProgram(_clipShaderPass1);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _histBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _histMaxBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, excessBuffer);
		glUniform1ui(glGetUniformLocation(_clipShaderPass1, "NUM_BINS"), _numOutGrayVals);
		glUniform1f(glGetUniformLocation(_clipShaderPass1, "clipLimit"), clipLimit);

		glDispatchCompute((GLuint)((histSize + 63) / 64), 1, 1);

		// make sure writting to the image is finished before reading 
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		glUseProgram(0);

		////////////////////////////////////////////////////////////////////////////
		// Clip the Histogram - Pass 2 
		// - redistribute the remaining excess pixels throughout the image
		std::cerr << "clip Hist 2... ";

		// compute stepSize for the second pass of redistributing the excess pixels
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, excessBuffer);
		excess = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
		uint32_t* stepSize = new uint32_t[numHistograms];
		memset(stepSize, 0, numHistograms * sizeof(uint32_t));

		bool computePass2 = false;
		for (unsigned int i = 0; i < numHistograms; i++) {
			if (excess[i] == 0) {
				stepSize[i] = 0;
			}
			else {
				stepSize[i] = std::max(_numInGrayVals / excess[i], 1u);
				computePass2 = true;
			}
		}
		glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

		if (computePass2) {
			GLuint stepSizeBuffer;
			glGenBuffers(1, &stepSizeBuffer);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, stepSizeBuffer);
			glBufferData(GL_SHADER_STORAGE_BUFFER, numHistograms * sizeof(uint32_t), stepSize, GL_STREAM_READ);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);


			glUseProgram(_clipShaderPass2);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _histBuffer);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _histMaxBuffer);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, excessBuffer);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, stepSizeBuffer);
			glUniform1ui(glGetUniformLocation(_clipShaderPass2, "NUM_BINS"), _numOutGrayVals);
			glUniform1f(glGetUniformLocation(_excessShader, "clipLimit"), clipLimit);


			glDispatchCompute((GLuint)((histSize + 63) / 64), 1, 1);

			// make sure writting to the image is finished before reading 
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			glUseProgram(0);

			glDeleteBuffers(1, &stepSizeBuffer);
		}
		else {
			std::cerr << " no need ";
		}

		delete[] stepSize;
		glDeleteBuffers(1, &excessBuffer);
	}

	////////////////////////////////////////////////////////////////////////////
	// Map the histograms 
	// - calculate the CDF for each of the histograms and store it in hist

	unsigned int numPixelsSB;
	if (numPixels == -1) {
		glm::vec3 sizeSB = volDims / numSB;
		numPixelsSB = sizeSB.x * sizeSB.y * sizeSB.z;
	}
	else {
		numPixelsSB = numPixels;
	}

	std::vector<std::thread> threads;

	uint32_t* hist = new uint32_t[histSize];
	memset(hist, 0, histSize * sizeof(uint32_t));
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _histBuffer);
	hist = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_WRITE);

	fprintf(stderr, "numHistograms: %d\n", numHistograms);
	for (unsigned int currHistIndex = 0; currHistIndex < numHistograms; currHistIndex++) {
		uint32_t* currHist = &hist[currHistIndex * _numOutGrayVals];
		threads.push_back(std::thread(mapHistogram, minMax[0], minMax[1], numPixelsSB, _numOutGrayVals, currHist));
	}
	for (auto & currThread : threads) {
		currThread.join();
	}
	     
	//fprintf(stderr, "Mapped Histogram\n");


	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

	printf("\n");
}

void ComputeCLAHE::computeMaskedClipHist(glm::uvec3 volDims, float clipLimit, uint32_t* min, uint32_t* max, uint32_t* numPixels) {

	uint32_t histSize = _numOutGrayVals;
	uint32_t numHistograms = _numOrgans;

	if (clipLimit < 1.0f) {
		std::cerr << "\nexcess ... ";
		////////////////////////////////////////////////////////////////////////////
		// Calculate the excess pixels based on the clipLimit


		// buffer for the pixels to re-distribute
		uint32_t* excess = new uint32_t[numHistograms];
		memset(excess, 0, numHistograms * sizeof(uint32_t));

		GLuint excessBuffer;
		glGenBuffers(1, &excessBuffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, excessBuffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, numHistograms * sizeof(uint32_t), excess, GL_STREAM_READ);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// Set up Compute Shader 
		glUseProgram(_excessShader);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _histBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _histMaxBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, excessBuffer);
		glUniform1ui(glGetUniformLocation(_excessShader, "NUM_BINS"), _numOutGrayVals);
		glUniform1f(glGetUniformLocation(_excessShader, "clipLimit"), clipLimit);

		glDispatchCompute((GLuint)((histSize + 63) / 64), 1, 1);

		// make sure writting to the image is finished before reading 
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		glUseProgram(0);
		delete[] excess;

		////////////////////////////////////////////////////////////////////////////
		// Clip the Histogram - Pass 1 
		// - clip the values and re-distribute some to all pixels

		std::cerr << "clip Hist 1... ";

		glUseProgram(_clipShaderPass1);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _histBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _histMaxBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, excessBuffer);
		glUniform1ui(glGetUniformLocation(_clipShaderPass1, "NUM_BINS"), _numOutGrayVals);
		glUniform1f(glGetUniformLocation(_clipShaderPass1, "clipLimit"), clipLimit);

		glDispatchCompute((GLuint)((histSize + 63) / 64), 1, 1);

		// make sure writting to the image is finished before reading 
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		glUseProgram(0);

		////////////////////////////////////////////////////////////////////////////
		// Clip the Histogram - Pass 2 
		// - redistribute the remaining excess pixels throughout the image
		std::cerr << "clip Hist 2... ";

		// compute stepSize for the second pass of redistributing the excess pixels
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, excessBuffer);
		excess = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
		uint32_t* stepSize = new uint32_t[numHistograms];
		memset(stepSize, 0, numHistograms * sizeof(uint32_t));

		bool computePass2 = false;
		for (unsigned int i = 0; i < numHistograms; i++) {
			if (excess[i] == 0) {
				stepSize[i] = 0;
			}
			else {
				stepSize[i] = std::max(_numInGrayVals / excess[i], 1u);
				computePass2 = true;
			}
		}
		glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

		if (computePass2) {
			GLuint stepSizeBuffer;
			glGenBuffers(1, &stepSizeBuffer);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, stepSizeBuffer);
			glBufferData(GL_SHADER_STORAGE_BUFFER, numHistograms * sizeof(uint32_t), stepSize, GL_STREAM_READ);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);


			glUseProgram(_clipShaderPass2);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _histBuffer);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _histMaxBuffer);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, excessBuffer);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, stepSizeBuffer);
			glUniform1ui(glGetUniformLocation(_clipShaderPass2, "NUM_BINS"), _numOutGrayVals);
			glUniform1f(glGetUniformLocation(_excessShader, "clipLimit"), clipLimit);


			glDispatchCompute((GLuint)((histSize + 63) / 64), 1, 1);

			// make sure writting to the image is finished before reading 
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			glUseProgram(0);

			glDeleteBuffers(1, &stepSizeBuffer);
		}
		else {
			std::cerr << " no need ";
		}

		delete[] stepSize;
		glDeleteBuffers(1, &excessBuffer);
	}

	////////////////////////////////////////////////////////////////////////////
	// Map the histograms 
	// - calculate the CDF for each of the histograms and store it in hist

	std::vector<std::thread> threads;

	uint32_t* hist = new uint32_t[histSize];
	memset(hist, 0, histSize * sizeof(uint32_t));
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _histBuffer);
	hist = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_WRITE);

	fprintf(stderr, "numHistograms: %d\n", numHistograms);
	for (unsigned int currHistIndex = 0; currHistIndex < numHistograms; currHistIndex++) {

		uint32_t* currHist = &hist[currHistIndex * _numOutGrayVals];
		uint32_t currMin = min[currHistIndex];
		uint32_t currMax = max[currHistIndex];
		uint32_t currPixelCount = numPixels[currHistIndex];

		threads.push_back(std::thread(mapHistogram, currMin, currMax, currPixelCount, _numOutGrayVals, currHist));
	}
	for (auto & currThread : threads) {
		currThread.join();
	}

	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

	printf("\n");
}


GLuint ComputeCLAHE::computeLerp(GLuint& maskedVersion, glm::uvec3 volDims, glm::uvec3 numSB, bool useMask, glm::uvec3 offset) {

	printf("lerp... (%d, %d, %d) ", numSB.x, numSB.y, numSB.z);

	// generate the new volume texture
	GLuint newVolumeTexture;
	glGenTextures(1, &newVolumeTexture);
	glBindTexture(GL_TEXTURE_3D, newVolumeTexture);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexStorage3D(GL_TEXTURE_3D, 1, GL_R16F, _volDims.x, _volDims.y, _volDims.z);
	glBindTexture(GL_TEXTURE_3D, 0);


	// Set up Compute Shader 
	glUseProgram(_lerpShader);
	glBindImageTexture(0, _volumeTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R16UI);
	glBindImageTexture(1, _maskTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R8UI);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _LUTbuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _histBuffer);
	glBindImageTexture(4, newVolumeTexture, 0, GL_TRUE, _layer, GL_WRITE_ONLY, GL_R16F);
	glUniform3i(glGetUniformLocation(_lerpShader, "numSB"), numSB.x, numSB.y, numSB.z);
	glUniform1ui(glGetUniformLocation(_lerpShader, "NUM_IN_BINS"), _numInGrayVals);
	glUniform1ui(glGetUniformLocation(_lerpShader, "NUM_OUT_BINS"), _numOutGrayVals);
	glUniform1i(glGetUniformLocation(_lerpShader, "useLUT"), _useLUT);
	glUniform1i(glGetUniformLocation(_lerpShader, "useMask"), useMask);

	glDispatchCompute(	(GLuint)((volDims.x + 3) / 4),
						(GLuint)((volDims.y + 3) / 4),
						(GLuint)((volDims.z + 3) / 4));

	// make sure writting to the image is finished before reading 
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glUseProgram(0);	
	

	if (!useMask) {
		printf("lerp round2 ...");
		// generate the new volume texture MASKED
		//GLuint maskedVersion;
		glGenTextures(1, &maskedVersion);
		glBindTexture(GL_TEXTURE_3D, maskedVersion);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexStorage3D(GL_TEXTURE_3D, 1, GL_R16F, _volDims.x, _volDims.y, _volDims.z);
		glBindTexture(GL_TEXTURE_3D, 0);


		// Set up Compute Shader 
		glUseProgram(_lerpShader);
		glBindImageTexture(0, _volumeTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R16UI);
		glBindImageTexture(1, _maskTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R8UI);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _LUTbuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _histBuffer);
		glBindImageTexture(4, maskedVersion, 0, GL_TRUE, _layer, GL_WRITE_ONLY, GL_R16F);
		glUniform3i(glGetUniformLocation(_lerpShader, "numSB"), numSB.x, numSB.y, numSB.z);
		glUniform1ui(glGetUniformLocation(_lerpShader, "NUM_BINS"), _numInGrayVals);
		glUniform3ui(glGetUniformLocation(_lerpShader, "offset"), offset.x, offset.y, offset.z);
		glUniform1i(glGetUniformLocation(_lerpShader, "useLUT"), _useLUT);
		glUniform1i(glGetUniformLocation(_lerpShader, "useMask"), true);

		glDispatchCompute((GLuint)((volDims.x + 3) / 4),
			(GLuint)((volDims.y + 3) / 4),
			(GLuint)((volDims.z + 3) / 4));

		// make sure writting to the image is finished before reading 
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
		glUseProgram(0);

		printf("...\n");
	}
	return newVolumeTexture;
}

GLuint ComputeCLAHE::computeFocusedLerp(glm::uvec3 volDims, glm::uvec3 numSB, glm::uvec3 minVal, glm::vec3 maxVal) {

	printf("lerp...");

	// generate the new volume texture
	GLuint newVolumeTexture;
	glGenTextures(1, &newVolumeTexture);
	glBindTexture(GL_TEXTURE_3D, newVolumeTexture);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexStorage3D(GL_TEXTURE_3D, 1, GL_R16F, _volDims.x, _volDims.y, _volDims.z);
	glBindTexture(GL_TEXTURE_3D, 0);


	// Set up Compute Shader 
	glUseProgram(_lerpFocusedShader);
	glBindImageTexture(0, _volumeTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R16UI);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _LUTbuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _histBuffer);
	glBindImageTexture(3, newVolumeTexture, 0, GL_TRUE, _layer, GL_WRITE_ONLY, GL_R16F);
	glUniform3i(glGetUniformLocation(_lerpFocusedShader, "numSB"), numSB.x, numSB.y, numSB.z);
	glUniform1ui(glGetUniformLocation(_lerpFocusedShader, "NUM_IN_BINS"), _numInGrayVals);
	glUniform1ui(glGetUniformLocation(_lerpFocusedShader, "NUM_OUT_BINS"), _numOutGrayVals);
	glUniform3ui(glGetUniformLocation(_lerpFocusedShader, "minVal"), minVal.x, minVal.y, minVal.z);
	glUniform3ui(glGetUniformLocation(_lerpFocusedShader, "maxVal"), maxVal.x, maxVal.y, maxVal.z);
	glUniform1i(glGetUniformLocation(_lerpFocusedShader, "useLUT"), _useLUT);

	glDispatchCompute(	(GLuint)((_volDims.x + 3) / 4),
						(GLuint)((_volDims.y + 3) / 4),
						(GLuint)((_volDims.z + 3) / 4));

	// make sure writting to the image is finished before reading 
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glUseProgram(0);

	printf("...\n");
	return newVolumeTexture;
}

GLuint ComputeCLAHE::computeMaskedLerp(glm::uvec3 volDims) {

	printf("Masked lerp... (%d organs) ", _numOrgans);

	// generate the new volume texture
	GLuint newVolumeTexture;
	glGenTextures(1, &newVolumeTexture);
	glBindTexture(GL_TEXTURE_3D, newVolumeTexture);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexStorage3D(GL_TEXTURE_3D, 1, GL_R16F, _volDims.x, _volDims.y, _volDims.z);
	glBindTexture(GL_TEXTURE_3D, 0);


	// Set up Compute Shader 
	glUseProgram(_lerpMaskedShader);
	glBindImageTexture(0, _volumeTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R16UI);
	glBindImageTexture(1, _maskTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R8UI);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _LUTbuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _histBuffer);
	glBindImageTexture(4, newVolumeTexture, 0, GL_TRUE, _layer, GL_WRITE_ONLY, GL_R16F);
	glUniform3i(glGetUniformLocation(_lerpMaskedShader, "numSB"), 1, 1, 1);
	glUniform1ui(glGetUniformLocation(_lerpMaskedShader, "NUM_IN_BINS"), _numInGrayVals);
	glUniform1ui(glGetUniformLocation(_lerpMaskedShader, "NUM_OUT_BINS"), _numOutGrayVals);
	glUniform1i(glGetUniformLocation(_lerpMaskedShader, "useLUT"), _useLUT);

	glDispatchCompute(	(GLuint)((volDims.x + 3) / 4),
						(GLuint)((volDims.y + 3) / 4),
						(GLuint)((volDims.z + 3) / 4));

	// make sure writting to the image is finished before reading 
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glUseProgram(0);

	printf("...\n"); 
	return newVolumeTexture;
}

////////////////////////////////////////////////////////////////////////////////
// Helper Methods
// multi-thread map hist across each histogram 

void mapHistogram(uint32_t minVal, uint32_t maxVal, uint32_t numPixelsSB, uint32_t numBins, uint32_t* localHist) {

	float sum = 0;
	const float scale = ((float)(maxVal - minVal)) / (float)numPixelsSB;
	//printf("min: %u, \tmax: %u, \tnumPixels: %u, \tnumBins: %u, scale: %f\n", minVal, maxVal, numPixelsSB, numBins, scale);

	// for each bin
	for (unsigned int i = 0; i < numBins; i++) {

		// add the histogram value for this contextual region to the sum 
		sum += localHist[i];

		// normalize the cdf
		localHist[i] = (unsigned int)(min(minVal + sum * scale, (float)maxVal));
	}
}

////////////////////////////////////////////////////////////////////////////////
// Helper Methods - Interaction

bool ComputeCLAHE::ChangePixelsPerSB(bool decrease) {

	if (decrease) {
		if (_pixelRatio == _minPixels) {
			printf("Pixel Ratio already smallest\n");
			return false;
		}
		_pixelRatio -= _minPixels;
	}
	else {
		if (_pixelRatio.x > _volDims.x&& _pixelRatio.y > _volDims.y&& _pixelRatio.z > _volDims.z) {
			printf("Pixel Ratio already largest\n");
			return false;
		}
		_pixelRatio += _minPixels;
	}

	_pixelRatio.x = std::max(_minPixels.x, _pixelRatio.x);
	_pixelRatio.y = std::max(_minPixels.y, _pixelRatio.y);
	_pixelRatio.z = std::max(_minPixels.z, _pixelRatio.z);

	printf("Pixel Ratio: %d, %d, %d\n", _pixelRatio.x, _pixelRatio.y, _pixelRatio.z);
	return true;
}

////////////////////////////////////////////////////////////////////////////////