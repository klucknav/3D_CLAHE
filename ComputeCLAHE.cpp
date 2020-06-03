////////////////////////////////////////
// ComputeCLAHE.cpp
////////////////////////////////////////

#include "ComputeCLAHE.h"
#include "Shader.h"

#include <thread>
#include <algorithm>

using namespace std;

void mapHistogram(uint32_t minVal, uint32_t maxVal, uint32_t numPixelsSB, uint32_t numBins, uint32_t* localHist);

////////////////////////////////////////////////////////////////////////////////
// Constructors/Destructors

ComputeCLAHE::ComputeCLAHE(GLuint volumeTexture, GLuint maskTexture, glm::ivec3 volDims, 
						unsigned int finalGrayVals, unsigned int inGrayVals, unsigned int numOrgans) {
	
	Init(volumeTexture, maskTexture, volDims, finalGrayVals, inGrayVals, numOrgans);
}

void ComputeCLAHE::Init(GLuint volumeTexture, GLuint maskTexture, glm::ivec3 volDims, 
						unsigned int finalGrayVals, unsigned int inGrayVals, unsigned int numOrgans) {
	
	// Load CLAHE/Focused CLAHE Shaders
	_minMaxShader = LoadComputeShader("minMax.comp");
	_LUTshader = LoadComputeShader("LUT.comp");
	_histShader = LoadComputeShader("hist.comp");
	_excessShader = LoadComputeShader("excess.comp");
	_clipShaderPass1 = LoadComputeShader("clipHist.comp");
	_clipShaderPass2 = LoadComputeShader("clipHist_p2.comp");
	_lerpShader = LoadComputeShader("lerp.comp");
	_lerpShader_Focused = LoadComputeShader("lerp_focused.comp");

	// Load Masked CLAHE Shaders
	_minMaxShader_Masked = LoadComputeShader("minMax_masked.comp");
	_LUTShader_Masked = LoadComputeShader("LUT_masked.comp");
	_histShader_Masked = LoadComputeShader("hist_masked.comp");
	_excessShader_Masked = LoadComputeShader("excess_masked.comp");
	_clipShaderPass1_Masked = LoadComputeShader("clipHist_masked.comp");
	_clipShaderPass2_Masked = LoadComputeShader("clipHist_p2_masked.comp");
	_lerpShader_Masked = LoadComputeShader("lerp_masked.comp");

	// Volume Data 
	_volumeTexture = volumeTexture;			_maskTexture = maskTexture;
	_numOutGrayVals = finalGrayVals;		_numInGrayVals = inGrayVals;
	_volDims = volDims;
	_numOrgans = numOrgans;
}

ComputeCLAHE::~ComputeCLAHE() {
	// Delete the Shaders
	glDeleteProgram(_minMaxShader);
	glDeleteProgram(_LUTshader);
	glDeleteProgram(_minMaxShader_Masked);
	glDeleteProgram(_LUTShader_Masked);

	glDeleteProgram(_histShader);
	glDeleteProgram(_histShader_Masked);

	glDeleteProgram(_excessShader);
	glDeleteProgram(_clipShaderPass1);
	glDeleteProgram(_clipShaderPass2);

	glDeleteProgram(_excessShader_Masked);
	glDeleteProgram(_clipShaderPass1_Masked);
	glDeleteProgram(_clipShaderPass2_Masked);

	glDeleteProgram(_lerpShader);
	glDeleteProgram(_lerpShader_Focused);
	glDeleteProgram(_lerpShader_Masked);

	// Delete the Buffers 
	glDeleteBuffers(1, &_LUTbuffer);
	glDeleteBuffers(1, &_histBuffer);
	glDeleteBuffers(1, &_histMaxBuffer);
}

////////////////////////////////////////////////////////////////////////////////
// CLAHE Functions

// 3D CLAHE
// numSB     - number of sub-blocks to use for 3D CLAHE
// clipLimit - [0,1] the smaller the value the lower the resulting contrast
//             0 returns the original volume
// Returns the new 3D CLAHE volume texture
GLuint ComputeCLAHE::Compute3D_CLAHE(glm::uvec3 numSB, float clipLimit) {

	printf("\n----- Compute 3D CLAHE ----- \n");

	// make sure the clip limit is valid - ie. between [0, 1]
	clipLimit = glm::clamp(clipLimit, 0.0f, 1.0f);
	// clipLimit == 0 -> return the original volume 
	if (clipLimit == 0) {
		return _volumeTexture;
	}
	// if the number of gray values is changing --> use the LUT
	bool useLUT = (_numOutGrayVals != _numInGrayVals);
	
	// Create the LUT
	uint32_t minMax[2] = { _numInGrayVals, 0 };
	computeLUT(_volDims, minMax, useLUT);

	// Create the Histograms
	computeHist(_volDims, numSB, useLUT);
	computeClipHist(_volDims, numSB, clipLimit, minMax);

	// Interpolate to create the new texture
	return computeLerp(_volDims, numSB, useLUT);
}

// Focused CLAHE
// min/max   - starting/ending point for Focused CLAHE
// clipLimit - [0,1] the smaller the value the lower the resulting contrast
//             0 returns the original volume
// Returns:
//		0 if the focused region is too small 
//		new Focused CLAHE volume texture 
GLuint ComputeCLAHE::ComputeFocused3D_CLAHE(glm::ivec3 min, glm::ivec3 max, float clipLimit) {

	printf("\n----- Compute Focused 3D CLAHE ----- \n");

	// Make sure Max/Min values are within the Volume Dimensions
	min.x = std::max(min.x, 0);				max.x = std::min(max.x, _volDims.x);
	min.y = std::max(min.y, 0);				max.y = std::min(max.y, _volDims.y);
	min.z = std::max(min.z, 0);				max.z = std::min(max.z, _volDims.z);

	// Determine the number of SB based on the dimensions of the Focused region
	glm::ivec3 focusedDim = max - min;
	if (focusedDim.x < _minPixels.x || focusedDim.y < _minPixels.y || focusedDim.z < _minPixels.z) {
		printf("Focused Region too Small\n");
		return 0;
	}
	unsigned int numSBx = std::max((focusedDim.x / _pixelRatio.x), 1);
	unsigned int numSBy = std::max((focusedDim.y / _pixelRatio.y), 1);
	unsigned int numSBz = std::max((focusedDim.z / _pixelRatio.z), 1);
	glm::uvec3 numSB = glm::uvec3(numSBx, numSBy, numSBz);

	// make sure the clip limit is valid - ie. between [0, 1]
	clipLimit = glm::clamp(clipLimit, 0.0f, 1.0f);
	// clipLimit == 0 -> return the original volume 
	if (clipLimit == 0) {
		return _volumeTexture;
	}
	// initialize variables 
	bool useLUT = true; // to spread out the pixel values for the focused region

	// Create the LUT
	uint32_t minMax[2] = { _numInGrayVals, 0 };
	computeLUT(focusedDim, minMax, useLUT, min);

	// Create the Histograms
	computeHist(focusedDim, numSB, useLUT, min);
	computeClipHist(focusedDim, numSB, clipLimit, minMax);

	// Interpolate to create the new texture
	return computeLerp_Focused(focusedDim, numSB, min, max, useLUT);
}

// Masked CLAHE
// clipLimit - [0,1] the smaller the value the lower the resulting contrast
//             0 returns the original volume
// Returns the new Masked CLAHE volume texture 
GLuint ComputeCLAHE::ComputeMasked3D_CLAHE(float clipLimit) {	
	
	printf("\n----- Compute Masked 3D CLAHE ----- \n");

	// make sure the clip limit is valid - ie. between [0, 1]
	clipLimit = glm::clamp(clipLimit, 0.0f, 1.0f);
	// clipLimit == 0 -> return the original volume 
	if (clipLimit == 0) {
		return _volumeTexture;
	}

	// Create the LUTs
	bool useLUT = true; // to spread out the pixel values for the masked organs
	uint32_t* minData = new uint32_t[_numOrgans];		std::fill_n(minData, _numOrgans, _numInGrayVals);
	uint32_t* maxData = new uint32_t[_numOrgans];		memset(maxData, 0, _numOrgans * sizeof(uint32_t));
	uint32_t* numPixels = new uint32_t[_numOrgans];		memset(numPixels, 0, _numOrgans * sizeof(uint32_t));
	computeLUT_Masked(_volDims, minData, maxData, numPixels);

	// Create the Histograms 
	glm::uvec3 numSB = glm::uvec3(1, 1, 1);	// only use 1 SB per organ
	computeHist_Masked(_volDims, useLUT);
	computeClipHist_Masked(_volDims, clipLimit, minData, maxData, numPixels);
	delete[] minData;
	delete[] maxData;
	delete[] numPixels;

	// Interpolate to re-create the new texture
	return computeLerp_Masked(_volDims, useLUT);
}

////////////////////////////////////////////////////////////////////////////////
// CLAHE Compute Shader Functions

// Used for CLAHE and Focused CLAHE
void ComputeCLAHE::computeLUT(glm::uvec3 volDims, uint32_t* minMax, bool useLUT, glm::uvec3 offset) {

	////////////////////////////////////////////////////////////////////////////
	// Calculate the Min/Max Values for the volume 

	// buffer to store the min/max
	GLuint globalMinMaxBuffer;
	glGenBuffers(1, &globalMinMaxBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, globalMinMaxBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 2 * sizeof(uint32_t), minMax, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	
	// Set up Compute Shader 
	glUseProgram(_minMaxShader);
	glBindImageTexture(0, _volumeTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R16UI);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, globalMinMaxBuffer);
	glUniform3ui(glGetUniformLocation(_minMaxShader, "offset"), offset.x, offset.y, offset.z);
	glUniform3ui(glGetUniformLocation(_minMaxShader, "volumeDims"), volDims.x, volDims.y, volDims.z);

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

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);


	////////////////////////////////////////////////////////////////////////////
	// Compute the LUT

	// buffer to store the LUT
	glGenBuffers(1, &_LUTbuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _LUTbuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, _numInGrayVals * sizeof(uint32_t), nullptr, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	if (useLUT) {
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
}
// Used for Masked CLAHE
void ComputeCLAHE::computeLUT_Masked(glm::uvec3 volDims, uint32_t* minData, uint32_t* maxData, uint32_t* pixelCount) {
	
	////////////////////////////////////////////////////////////////////////////
	// Calculate the Min/Max Values for the volume 

	// buffer to store the min/max
	GLuint globalMinBuffer;
	uint32_t* tempMinData = new uint32_t[_numOrgans];
	std::fill_n(tempMinData, _numOrgans, _numInGrayVals);
	glGenBuffers(1, &globalMinBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, globalMinBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, _numOrgans * sizeof(uint32_t), tempMinData, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	GLuint globalMaxBuffer;
	uint32_t* tempMaxData = new uint32_t[_numOrgans];	
	memset(tempMaxData, 0, _numOrgans * sizeof(uint32_t));
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
	glUseProgram(_minMaxShader_Masked);
	glBindImageTexture(0, _volumeTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R16UI);
	glBindImageTexture(1, _maskTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R8UI);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, globalMinBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, globalMaxBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, unMaskedPixelBuffer);
	glUniform3ui(glGetUniformLocation(_minMaxShader_Masked, "volumeDims"), volDims.x, volDims.y, volDims.z);

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
	}

	// Get the num Pixels in the masked region
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, unMaskedPixelBuffer);
	tempPixelCount = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

	for (int i = 0; i < _numOrgans; i++) {
		pixelCount[i] = tempPixelCount[i];
	}

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);


	////////////////////////////////////////////////////////////////////////////
	// Compute the LUT

	// buffer to store the LUT
	glGenBuffers(1, &_LUTbuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _LUTbuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, _numInGrayVals * _numOrgans * sizeof(uint32_t), nullptr, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Set up Compute Shader 
	glUseProgram(_LUTShader_Masked);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, globalMinBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, globalMaxBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _LUTbuffer);
	glUniform1ui(glGetUniformLocation(_LUTShader_Masked, "NUM_IN_BINS"), _numInGrayVals);
	glUniform1ui(glGetUniformLocation(_LUTShader_Masked, "NUM_OUT_BINS"), _numOutGrayVals);
	glUniform1ui(glGetUniformLocation(_LUTShader_Masked, "numOrgans"), _numOrgans);

	glDispatchCompute((GLuint)(_numInGrayVals / 64), 1, 1);

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	glUseProgram(0);

	// clean up
	glDeleteBuffers(1, &globalMinBuffer);
	glDeleteBuffers(1, &globalMaxBuffer);
}

// Used for CLAHE and Focused CLAHE
void ComputeCLAHE::computeHist(glm::uvec3 volDims, glm::uvec3 numSB, bool useLUT, glm::uvec3 offset) {
	
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
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _LUTbuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _histBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _histMaxBuffer);
	glUniform3i(glGetUniformLocation(_histShader, "numSB"), numSB.x, numSB.y, numSB.z);
	glUniform1ui(glGetUniformLocation(_histShader, "NUM_OUT_BINS"), _numOutGrayVals);
	glUniform3ui(glGetUniformLocation(_histShader, "offset"), offset.x, offset.y, offset.z);
	glUniform1i(glGetUniformLocation(_histShader, "useLUT"), useLUT);
	glUniform3ui(glGetUniformLocation(_histShader, "volumeDims"), volDims.x, volDims.y, volDims.z);

	glDispatchCompute(	(GLuint)((volDims.x + 3) / 4),
						(GLuint)((volDims.y + 3) / 4),
						(GLuint)((volDims.z + 3) / 4));

	// make sure writting to the image is finished before reading 
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glUseProgram(0);
}
// Used for Masked CLAHE
void ComputeCLAHE::computeHist_Masked(glm::uvec3 volDims, bool useLUT) {
	
	// Buffer to store the Histograms
	uint32_t histSize = _numOutGrayVals * _numOrgans;
	glGenBuffers(1, &_histBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _histBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, histSize * sizeof(uint32_t), nullptr, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Buffer to store the max values of the histograms
	uint32_t maxValSize = _numOrgans;
	uint32_t* _histMax = new uint32_t[maxValSize];
	memset(_histMax, 0, maxValSize * sizeof(uint32_t));
	glGenBuffers(1, &_histMaxBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _histMaxBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, maxValSize * sizeof(uint32_t), _histMax, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Set up Compute Shader 
	glUseProgram(_histShader_Masked);
	glBindImageTexture(0, _volumeTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R16UI);
	glBindImageTexture(1, _maskTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R8UI);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _LUTbuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _histBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _histMaxBuffer);
	glUniform1ui(glGetUniformLocation(_histShader_Masked, "NUM_BINS"), _numInGrayVals);
	glUniform1i(glGetUniformLocation(_histShader_Masked, "useLUT"), useLUT);
	glUniform3ui(glGetUniformLocation(_histShader_Masked, "volumeDims"), volDims.x, volDims.y, volDims.z);

	glDispatchCompute(	(GLuint)((volDims.x + 3) / 4),
						(GLuint)((volDims.y + 3) / 4),
						(GLuint)((volDims.z + 3) / 4));

	// make sure writting to the image is finished before reading 
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glUseProgram(0);
}

// Used for CLAHE and Focused CLAHE
void ComputeCLAHE::computeClipHist(glm::uvec3 volDims, glm::uvec3 numSB, float clipLimit, uint32_t* minMax, int numPixels) {

	uint32_t histSize = _numOutGrayVals * numSB.x * numSB.y * numSB.z;
	uint32_t numHistograms = numSB.x * numSB.y * numSB.z;

	if (clipLimit < 1.0f) {

		////////////////////////////////////////////////////////////////////////////
		// Calculate the excess pixels based on the clipLimit
		
		// buffer for the pixels to re-distribute
		GLuint excessBuffer;
		uint32_t* excess = new uint32_t[numHistograms];
		memset(excess, 0, numHistograms * sizeof(uint32_t));
		glGenBuffers(1, &excessBuffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, excessBuffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, numHistograms * sizeof(uint32_t), excess, GL_STREAM_READ);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		delete[] excess;

		// calculate the minClipValue
		glm::uvec3 sizeSB = volDims / numSB;
		float tempClipValue = 1.1f * (sizeSB.x * sizeSB.y * sizeSB.z) / _numOutGrayVals;
		unsigned int minClipValue = unsigned int(tempClipValue + 0.5f);

		// Set up Compute Shader 
		glUseProgram(_excessShader);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _histBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _histMaxBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, excessBuffer);
		glUniform1ui(glGetUniformLocation(_excessShader, "NUM_BINS"), _numOutGrayVals);
		glUniform1f(glGetUniformLocation(_excessShader, "clipLimit"), clipLimit);
		glUniform1ui(glGetUniformLocation(_excessShader, "minClipValue"), minClipValue);

		int width = 4096;
		int count = (histSize + 63) / 64;
		GLuint dispatchWidth = count / (width*width);
		GLuint dispatchHeight = (count / width) % width;
		GLuint dispatchDepth = count % width;
		glDispatchCompute(dispatchWidth, dispatchHeight, dispatchDepth);

		// make sure writting to the image is finished before reading 
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		glUseProgram(0);

		////////////////////////////////////////////////////////////////////////////
		// Clip the Histogram - Pass 1 
		// - clip the values and re-distribute to all pixels

		glUseProgram(_clipShaderPass1);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _histBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _histMaxBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, excessBuffer);
		glUniform1ui(glGetUniformLocation(_clipShaderPass1, "NUM_BINS"), _numOutGrayVals);
		glUniform1f(glGetUniformLocation(_clipShaderPass1, "clipLimit"), clipLimit);
		glUniform1ui(glGetUniformLocation(_clipShaderPass1, "minClipValue"), minClipValue);

		glDispatchCompute(dispatchWidth, dispatchHeight, dispatchDepth);

		// make sure writting to the image is finished before reading 
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		glUseProgram(0);

		////////////////////////////////////////////////////////////////////////////
		// Clip the Histogram - Pass 2 
		// - redistribute any remaining excess pixels throughout the image

		// Get the excess pixels
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, excessBuffer);
		excess = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
		glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

		// compute stepSize for the second pass of redistributing the excess pixels
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
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

		// if there were any excess pixels left
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
			glUniform1f(glGetUniformLocation(_clipShaderPass2, "clipLimit"), clipLimit);
			glUniform1ui(glGetUniformLocation(_clipShaderPass2, "minClipValue"), minClipValue);

			glDispatchCompute((GLuint)((histSize + 63) / 64), 1, 1);

			// make sure writting to the image is finished before reading 
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			glUseProgram(0);

			glDeleteBuffers(1, &stepSizeBuffer);
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

	uint32_t* hist = new uint32_t[histSize];
	memset(hist, 0, histSize * sizeof(uint32_t));
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _histBuffer);
	hist = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_WRITE);

	std::vector<std::thread> threads;
	for (unsigned int currHistIndex = 0; currHistIndex < numHistograms; currHistIndex++) {
		uint32_t* currHist = &hist[currHistIndex * _numOutGrayVals];
		threads.push_back(std::thread(mapHistogram, minMax[0], minMax[1], numPixelsSB, _numOutGrayVals, currHist));
	}
	for (auto & currThread : threads) {
		currThread.join();
	}

	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

}
// Used for Masked CLAHE
void ComputeCLAHE::computeClipHist_Masked(glm::uvec3 volDims, float clipLimit, uint32_t* min, uint32_t* max, uint32_t* numPixels) {

	uint32_t histSize = _numOutGrayVals;
	uint32_t numHistograms = _numOrgans;

	if (clipLimit < 1.0f) {
		////////////////////////////////////////////////////////////////////////////
		// Calculate the excess pixels based on the clipLimit

		// buffer for the pixels to re-distribute
		GLuint excessBuffer;
		uint32_t* excess = new uint32_t[numHistograms];
		memset(excess, 0, numHistograms * sizeof(uint32_t));
		glGenBuffers(1, &excessBuffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, excessBuffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, numHistograms * sizeof(uint32_t), excess, GL_STREAM_READ);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		delete[] excess;

		GLuint minClipValueBuffer;
		// calculate the minClipValues for each Organ
		uint32_t* minClipValues = new uint32_t[_numOrgans];		
		memset(minClipValues, 0, _numOrgans * sizeof(uint32_t));
		for (int i = 0; i < _numOrgans; i++) {
			float tempClipValue = 1.1f * numPixels[i] / _numOutGrayVals;
			minClipValues[i] = unsigned int(tempClipValue + 0.5f);
		}
		glGenBuffers(1, &minClipValueBuffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, minClipValueBuffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, numHistograms * sizeof(uint32_t), minClipValues, GL_STREAM_READ);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// Set up Compute Shader 
		glUseProgram(_excessShader_Masked);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _histBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _histMaxBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, minClipValueBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, excessBuffer);
		glUniform1ui(glGetUniformLocation(_excessShader_Masked, "NUM_BINS"), _numOutGrayVals);
		glUniform1f(glGetUniformLocation(_excessShader_Masked, "clipLimit"), clipLimit);

		glDispatchCompute((GLuint)((histSize + 63) / 64), 1, 1);

		// make sure writting to the image is finished before reading 
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		glUseProgram(0);

		////////////////////////////////////////////////////////////////////////////
		// Clip the Histogram - Pass 1 
		// - clip the values and re-distribute some to all pixels
		
		glUseProgram(_clipShaderPass1_Masked);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _histBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _histMaxBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, excessBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, minClipValueBuffer);
		glUniform1ui(glGetUniformLocation(_clipShaderPass1_Masked, "NUM_BINS"), _numOutGrayVals);
		glUniform1f(glGetUniformLocation(_clipShaderPass1_Masked, "clipLimit"), clipLimit);

		glDispatchCompute((GLuint)((histSize + 63) / 64), 1, 1);

		// make sure writting to the image is finished before reading 
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		glUseProgram(0);

		////////////////////////////////////////////////////////////////////////////
		// Clip the Histogram - Pass 2 
		// - redistribute the remaining excess pixels throughout the image

		// Get the excess pixels
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, excessBuffer);
		excess = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
		glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

		// compute stepSize for the second pass of redistributing the excess pixels
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
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

		if (computePass2) {
			GLuint stepSizeBuffer;
			glGenBuffers(1, &stepSizeBuffer);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, stepSizeBuffer);
			glBufferData(GL_SHADER_STORAGE_BUFFER, numHistograms * sizeof(uint32_t), stepSize, GL_STREAM_READ);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

			glUseProgram(_clipShaderPass2_Masked);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _histBuffer);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _histMaxBuffer);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, excessBuffer);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, minClipValueBuffer);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, stepSizeBuffer);
			glUniform1ui(glGetUniformLocation(_clipShaderPass2_Masked, "NUM_BINS"), _numOutGrayVals);
			glUniform1f(glGetUniformLocation(_clipShaderPass2_Masked, "clipLimit"), clipLimit);


			glDispatchCompute((GLuint)((histSize + 63) / 64), 1, 1);

			// make sure writting to the image is finished before reading 
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			glUseProgram(0);

			glDeleteBuffers(1, &stepSizeBuffer);
		}

		//delete[] excess;
		delete[] stepSize;
		delete[] minClipValues;
		glDeleteBuffers(1, &excessBuffer);
	}

	////////////////////////////////////////////////////////////////////////////
	// Map the histograms 
	// - calculate the CDF for each of the histograms and store it in hist


	uint32_t* hist = new uint32_t[histSize];
	memset(hist, 0, histSize * sizeof(uint32_t));
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _histBuffer);
	hist = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_WRITE);
	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

	std::vector<std::thread> threads;
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

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
}

// Used for CLAHE
GLuint ComputeCLAHE::computeLerp(glm::uvec3 volDims, glm::uvec3 numSB, bool useLUT, glm::uvec3 offset) {

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
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _LUTbuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _histBuffer);
	glBindImageTexture(3, newVolumeTexture, 0, GL_TRUE, _layer, GL_WRITE_ONLY, GL_R16F);
	glUniform3i(glGetUniformLocation(_lerpShader, "numSB"), numSB.x, numSB.y, numSB.z);
	glUniform1ui(glGetUniformLocation(_lerpShader, "NUM_IN_BINS"), _numInGrayVals);
	glUniform1ui(glGetUniformLocation(_lerpShader, "NUM_OUT_BINS"), _numOutGrayVals);
	glUniform1i(glGetUniformLocation(_lerpShader, "useLUT"), useLUT);

	glDispatchCompute(	(GLuint)((volDims.x + 3) / 4),
						(GLuint)((volDims.y + 3) / 4),
						(GLuint)((volDims.z + 3) / 4));

	// make sure writting to the image is finished before reading 
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glUseProgram(0);	
	
	return newVolumeTexture;
}
// Used for Focused CLAHE
GLuint ComputeCLAHE::computeLerp_Focused(glm::uvec3 volDims, glm::uvec3 numSB, glm::uvec3 minVal, glm::vec3 maxVal, bool useLUT) {

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
	glUseProgram(_lerpShader_Focused);
	glBindImageTexture(0, _volumeTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R16UI);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _LUTbuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _histBuffer);
	glBindImageTexture(3, newVolumeTexture, 0, GL_TRUE, _layer, GL_WRITE_ONLY, GL_R16F);
	glUniform3i(glGetUniformLocation(_lerpShader_Focused, "numSB"), numSB.x, numSB.y, numSB.z);
	glUniform1ui(glGetUniformLocation(_lerpShader_Focused, "NUM_IN_BINS"), _numInGrayVals);
	glUniform1ui(glGetUniformLocation(_lerpShader_Focused, "NUM_OUT_BINS"), _numOutGrayVals);
	glUniform3ui(glGetUniformLocation(_lerpShader_Focused, "minVal"), minVal.x, minVal.y, minVal.z);
	glUniform3ui(glGetUniformLocation(_lerpShader_Focused, "maxVal"), maxVal.x, maxVal.y, maxVal.z);
	glUniform1i(glGetUniformLocation(_lerpShader_Focused, "useLUT"), useLUT);

	glDispatchCompute(	(GLuint)((_volDims.x + 3) / 4),
						(GLuint)((_volDims.y + 3) / 4),
						(GLuint)((_volDims.z + 3) / 4));

	// make sure writting to the image is finished before reading 
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glUseProgram(0);

	return newVolumeTexture;
}
// Used for Masked CLAHE
GLuint ComputeCLAHE::computeLerp_Masked(glm::uvec3 volDims, bool useLUT) {
	
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
	glUseProgram(_lerpShader_Masked);
	glBindImageTexture(0, _volumeTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R16UI);
	glBindImageTexture(1, _maskTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R8UI);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _LUTbuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _histBuffer);
	glBindImageTexture(4, newVolumeTexture, 0, GL_TRUE, _layer, GL_WRITE_ONLY, GL_R16F);
	glUniform3i(glGetUniformLocation(_lerpShader_Masked, "numSB"), 1, 1, 1);
	glUniform1ui(glGetUniformLocation(_lerpShader_Masked, "NUM_IN_BINS"), _numInGrayVals);
	glUniform1ui(glGetUniformLocation(_lerpShader_Masked, "NUM_OUT_BINS"), _numOutGrayVals);
	glUniform1i(glGetUniformLocation(_lerpShader_Masked, "useLUT"), useLUT);

	glDispatchCompute(	(GLuint)((volDims.x + 3) / 4),
						(GLuint)((volDims.y + 3) / 4),
						(GLuint)((volDims.z + 3) / 4));

	// make sure writting to the image is finished before reading 
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glUseProgram(0);

	return newVolumeTexture;
}

////////////////////////////////////////////////////////////////////////////////
// Helper Method - multi-thread map hist across each histogram 

void mapHistogram(uint32_t minVal, uint32_t maxVal, uint32_t numPixelsSB, uint32_t numBins, uint32_t* localHist) {

	float sum = 0;
	const float scale = ((float)(maxVal - minVal)) / (float)numPixelsSB;

	// for each bin
	for (unsigned int i = 0; i < numBins; i++) {

		// add the histogram value for this contextual region to the sum 
		sum += localHist[i];

		// normalize the cdf
		localHist[i] = (unsigned int)(min(minVal + sum * scale, (float)maxVal));
	}
}

////////////////////////////////////////////////////////////////////////////////
// Helper Method - Interaction with the number of SB for Focused CLAHE

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