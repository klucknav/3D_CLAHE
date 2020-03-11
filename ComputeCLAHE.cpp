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
	_LUTshader = LoadComputeShader("LUT.comp");
	_histShader = LoadComputeShader("hist.comp");
	_excessShader = LoadComputeShader("excess.comp");
	_clipShaderPass1 = LoadComputeShader("clipHist.comp");
	_clipShaderPass2 = LoadComputeShader("clipHist_p2.comp");
	_lerpShader = LoadComputeShader("lerp.comp");
	_focusedLerpShader = LoadComputeShader("lerp_focused.comp");

	// Volume Data 
	_volumeTexture = volumeTexture;
	_volDims = volDims;
	_numFinalGrayVals = finalGrayVals;
	_numInGrayVals = inGrayVals;
	
	// if the number of gray values is changing --> use the LUT
	_useLUT = (_numFinalGrayVals != _numInGrayVals);
	printf("useLUT: %s\n", _useLUT ? "true" : "false");
}

void ComputeCLAHE::Init(GLuint volumeTexture, glm::ivec3 volDims, unsigned int finalGrayVals, unsigned int inGrayVals) {
	
	// Load Shaders
	_minMaxShader = LoadComputeShader("minMax.comp");
	_LUTshader = LoadComputeShader("LUT.comp");
	_histShader = LoadComputeShader("hist.comp");
	_excessShader = LoadComputeShader("excess.comp");
	_clipShaderPass1 = LoadComputeShader("clipHist.comp");
	_clipShaderPass2 = LoadComputeShader("clipHist_p2.comp");
	_lerpShader = LoadComputeShader("lerp.comp");
	_focusedLerpShader = LoadComputeShader("lerp_focused.comp");

	// Volume Data 
	_volumeTexture = volumeTexture;
	_volDims = volDims;
	_numFinalGrayVals = finalGrayVals;
	_numInGrayVals = inGrayVals;

	// if the number of gray values is changing --> use the LUT
	_useLUT = (_numFinalGrayVals != _numInGrayVals);
	printf("useLUT: %s\n", _useLUT ? "true" : "false");
}

ComputeCLAHE::~ComputeCLAHE() {
	glDeleteProgram(_minMaxShader);
	glDeleteProgram(_LUTshader);
	glDeleteProgram(_histShader);
	glDeleteProgram(_excessShader);
	glDeleteProgram(_clipShaderPass1);
	glDeleteProgram(_clipShaderPass2);
	glDeleteProgram(_lerpShader);
}

////////////////////////////////////////////////////////////////////////////////
// CLAHE Functions

GLuint ComputeCLAHE::Compute3D_CLAHE(glm::uvec3 numSB, float clipLimit) {

	printf("\n----- Compute 3D CLAHE -----\n");
	printf("using (%u, %u, %u) subBlocks\n", numSB.x, numSB.y, numSB.z);

	chrono::high_resolution_clock::time_point t1 = chrono::high_resolution_clock::now();

	// make sure the clip limit is valid
	clipLimit = std::max(std::min(clipLimit, _maxClipLimit), _minClipLimit);
	printf("clipLimit: %.2f, numSubBlocks(%u, %u, %u)\n", clipLimit, numSB.x, numSB.y, numSB.z);

	// Create the LUT
	uint32_t minMax[2] = { _numInGrayVals, 0 };
	computeLUT(_volDims, minMax);
	printf("Min/max: (%d, %d)\n", minMax[0], minMax[1]);

	// Create the Histograms
	computeHist(_volDims, numSB);
	computeClipHist(_volDims, numSB, clipLimit, minMax);

	// Interpolate to re-create the new texture
	GLuint newTexture = computeLerp(_volDims, numSB);

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
	//unsigned int numSBx = std::max((focusedDim.x / _pixelPerSB), 1);
	//unsigned int numSBy = std::max((focusedDim.y / _pixelPerSB), 1);
	//unsigned int numSBz = std::max((focusedDim.z / _pixelPerSB), 1);
	glm::uvec3 numSB = glm::uvec3(numSBx, numSBy, numSBz);

	// make sure the clip limit is valid
	if (clipLimit < _minClipLimit || clipLimit > _maxClipLimit) {
		printf("ClipLimit out of bounds (%.3f)\n", clipLimit);
		return 0;
	}
	clipLimit = std::max(std::min(clipLimit, _maxClipLimit), _minClipLimit);
	printf("clipLimit: %.2f, numSubBlocks(%u, %u, %u)\n", clipLimit, numSB.x, numSB.y, numSB.z);


	// Create the LUT
	uint32_t minMax[2] = { _numInGrayVals, 0 };
	computeLUT(focusedDim, minMax, min);
	printf("Min/max: (%d, %d)\n", minMax[0], minMax[1]);

	// Creat the Histograms
	computeHist(focusedDim, numSB, min);
	computeClipHist(focusedDim, numSB, clipLimit, minMax);

	// Interpolate to re-create the new texture
	GLuint newTexture = computeFocusedLerp(focusedDim, numSB, min, max);

	chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();
	chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - t1);
	printf("Focused 3D CLAHE took: %f seconds\n\n", time_span.count());

	return newTexture;
}

////////////////////////////////////////////////////////////////////////////////
// CLAHE Compute Shader Functions

void ComputeCLAHE::computeLUT(glm::uvec3 volDims, uint32_t* minMax, glm::uvec3 offset) {

	printf("LUT...");

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
	glUniform1ui(glGetUniformLocation(_minMaxShader, "NUM_BINS"), _numInGrayVals);
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

	if (_useLUT) {
		// Set up Compute Shader 
		glUseProgram(_LUTshader);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, globalMinMaxBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _LUTbuffer);
		glUniform1ui(glGetUniformLocation(_LUTshader, "NUM_OUT_BINS"), _numFinalGrayVals);

		glDispatchCompute((GLuint)(_numInGrayVals / 64), 1, 1);

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		glUseProgram(0);
	}
	// clean up
	glDeleteBuffers(1, &globalMinMaxBuffer);

}

void ComputeCLAHE::computeHist(glm::uvec3 volDims, glm::uvec3 numSB, glm::uvec3 offset) {

	printf("\nCompute Hist...");

	// Buffer to store the Histograms
	uint32_t histSize = _numInGrayVals * numSB.x * numSB.y * numSB.z;
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
	glUniform1ui(glGetUniformLocation(_histShader, "NUM_BINS"), _numInGrayVals);
	glUniform3ui(glGetUniformLocation(_histShader, "offset"), offset.x, offset.y, offset.z);
	glUniform1i(glGetUniformLocation(_histShader, "useLUT"), _useLUT);
	glUniform3ui(glGetUniformLocation(_histShader, "volumeDims"), volDims.x, volDims.y, volDims.z);

	glDispatchCompute(	(GLuint)((volDims.x + 3) / 4),
						(GLuint)((volDims.y + 3) / 4),
						(GLuint)((volDims.z + 3) / 4));

	// make sure writting to the image is finished before reading 
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glUseProgram(0);
}

void ComputeCLAHE::computeClipHist(glm::uvec3 volDims, glm::uvec3 numSB, float clipLimit, uint32_t* minMax) {

	uint32_t histSize = _numInGrayVals * numSB.x * numSB.y * numSB.z;
	uint32_t numHistograms = numSB.x * numSB.y * numSB.z;

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
		glUniform1ui(glGetUniformLocation(_excessShader, "NUM_BINS"), _numInGrayVals);
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
		glUniform1ui(glGetUniformLocation(_clipShaderPass1, "NUM_BINS"), _numInGrayVals);
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
			glUniform1ui(glGetUniformLocation(_clipShaderPass2, "NUM_BINS"), _numInGrayVals);
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

	glm::vec3 sizeSB = volDims / numSB;
	unsigned int numPixelsSB = sizeSB.x * sizeSB.y * sizeSB.z;

	std::vector<std::thread> threads;

	uint32_t* hist = new uint32_t[histSize];
	memset(hist, 0, histSize * sizeof(uint32_t));
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _histBuffer);
	hist = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_WRITE);

	for (unsigned int currHistIndex = 0; currHistIndex < numHistograms; currHistIndex++) {
		uint32_t* currHist = &hist[currHistIndex * _numInGrayVals];
		threads.push_back(std::thread(mapHistogram, minMax[0], minMax[1], numPixelsSB, _numInGrayVals, currHist));
	}
	for (auto & currThread : threads) {
		currThread.join();
	}

	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

	printf("\n");
}

GLuint ComputeCLAHE::computeLerp(glm::uvec3 volDims, glm::uvec3 numSB, glm::uvec3 offset) {

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
	glUseProgram(_lerpShader);
	glBindImageTexture(0, _volumeTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R16UI);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _LUTbuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _histBuffer);
	glBindImageTexture(3, newVolumeTexture, 0, GL_TRUE, _layer, GL_WRITE_ONLY, GL_R16F);
	glUniform3i(glGetUniformLocation(_lerpShader, "numSB"), numSB.x, numSB.y, numSB.z);
	glUniform1ui(glGetUniformLocation(_lerpShader, "NUM_BINS"), _numInGrayVals);
	glUniform3ui(glGetUniformLocation(_lerpShader, "offset"), offset.x, offset.y, offset.z);
	glUniform1i(glGetUniformLocation(_lerpShader, "useLUT"), _useLUT);

	glDispatchCompute(	(GLuint)((volDims.x + 3) / 4),
						(GLuint)((volDims.y + 3) / 4),
						(GLuint)((volDims.z + 3) / 4));

	// make sure writting to the image is finished before reading 
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glUseProgram(0);

	printf("...\n");
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
	glUseProgram(_focusedLerpShader);
	glBindImageTexture(0, _volumeTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R16UI);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _LUTbuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _histBuffer);
	glBindImageTexture(3, newVolumeTexture, 0, GL_TRUE, _layer, GL_WRITE_ONLY, GL_R16F);
	glUniform3i(glGetUniformLocation(_focusedLerpShader, "numSB"), numSB.x, numSB.y, numSB.z);
	glUniform1ui(glGetUniformLocation(_focusedLerpShader, "NUM_IN_BINS"), _numInGrayVals);
	glUniform1ui(glGetUniformLocation(_focusedLerpShader, "NUM_OUT_BINS"), _numFinalGrayVals);
	glUniform3ui(glGetUniformLocation(_focusedLerpShader, "minVal"), minVal.x, minVal.y, minVal.z);
	glUniform3ui(glGetUniformLocation(_focusedLerpShader, "maxVal"), maxVal.x, maxVal.y, maxVal.z);
	glUniform1i(glGetUniformLocation(_focusedLerpShader, "useLUT"), _useLUT);

	glDispatchCompute(	(GLuint)((_volDims.x + 3) / 4),
						(GLuint)((_volDims.y + 3) / 4),
						(GLuint)((_volDims.z + 3) / 4));

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