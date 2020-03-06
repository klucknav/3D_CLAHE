////////////////////////////////////////
// ComputeCLAHE.h
// Compute CLAHE using Compute Shaders and multi-threading
////////////////////////////////////////

#pragma once

#include "core.h"

class ComputeCLAHE {
private:

	// compute shaders
	GLuint _minMaxShader, _LUTshader;
	GLuint _histShader, _excessShader, _clipShader1, _clipShader2;
	GLuint _lerpShader, _focusedLerpShader;

	// volume data 
	GLuint _volumeTexture;
	glm::vec3 _volDims;
	unsigned int _numFinalGrayVals;
	unsigned int _numInGrayVals;

	// CLAHE Data
	bool _useLUT = true;

	// calculated data
	uint32_t _globalMinMax[2];
	GLuint _newVolumeBuffer;
	GLuint _newVolumeTexture;

	// buffers 
	GLuint _globalMinMaxBuffer, _LUTbuffer, _histBuffer;
	GLuint _histMaxBuffer, _excessBuffer;
	GLuint _layer = 1;

	// CLAHE Compute Shader Functions
	void computeMinMax(glm::uvec3 volDims, glm::uvec3 offset = glm::uvec3(0));
	void computeLUT();
	void computeHist(glm::uvec3 volDims, glm::uvec3 numSB, glm::uvec3 offset = glm::uvec3(0));
	void computeClipHist(glm::uvec3 volDims, glm::uvec3 numSB, float clipLimit);
	GLuint computeLerp(glm::uvec3 volDims, glm::uvec3 numSB, glm::uvec3 offset = glm::uvec3(0));
	GLuint computeFocusedLerp(glm::uvec3 volDims, glm::uvec3 numSB, glm::uvec3 minVal, glm::vec3 maxVal);

public:
	ComputeCLAHE(GLuint volumeTexture, glm::vec3 volDims, unsigned int numFinalGrayVals, unsigned int numInGrayVals);
	~ComputeCLAHE();

	// CLAHE Methods
	GLuint Compute3D_CLAHE(glm::uvec3 numSB, float clipLimit);
	GLuint ComputeFocused3D_CLAHE(glm::uvec3 min, glm::uvec3 max, float clipLimit);

};