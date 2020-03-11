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
	GLuint _histShader, _excessShader, _clipShaderPass1, _clipShaderPass2;
	GLuint _lerpShader, _focusedLerpShader;

	// volume data 
	GLuint _volumeTexture;
	glm::ivec3 _volDims;
	unsigned int _numFinalGrayVals;
	unsigned int _numInGrayVals;

	// CLAHE Buffers and Data
	GLuint _LUTbuffer, _histBuffer, _histMaxBuffer;
	GLuint _layer = 1;
	bool _useLUT;

	// CLAHE Parameter Limits
	float _minClipLimit = 0.1f;
	float _maxClipLimit = 1.0f;
	int _pixelPerSB = 100;
	glm::ivec3 _pixelRatio = glm::ivec3(100, 100, 50);
	glm::ivec3 _minPixels = glm::ivec3(50, 50, 25);

	// CLAHE Compute Shader Functions
	void computeLUT(glm::uvec3 volDims, uint32_t* minMax, glm::uvec3 offset = glm::uvec3(0));
	void computeHist(glm::uvec3 volDims, glm::uvec3 numSB, glm::uvec3 offset = glm::uvec3(0));
	void computeClipHist(glm::uvec3 volDims, glm::uvec3 numSB, float clipLimit, uint32_t* minMax);
	GLuint computeLerp(glm::uvec3 volDims, glm::uvec3 numSB, glm::uvec3 offset = glm::uvec3(0));
	GLuint computeFocusedLerp(glm::uvec3 volDims, glm::uvec3 numSB, glm::uvec3 minVal, glm::vec3 maxVal);

public:
	ComputeCLAHE() {};
	ComputeCLAHE(GLuint volumeTexture, glm::ivec3 volDims, unsigned int finalGrayVals, unsigned int inGrayVals);
	~ComputeCLAHE();

	void Init(GLuint volumeTexture, glm::ivec3 volDims, unsigned int finalGrayVals, unsigned int inGrayVals);

	// CLAHE Methods
	GLuint Compute3D_CLAHE(glm::uvec3 numSB, float clipLimit);
	GLuint ComputeFocused3D_CLAHE(glm::ivec3 min, glm::ivec3 max, float clipLimit);

	// Change parameters for Focused CLAHE
	bool ChangePixelsPerSB(bool decrease);
};