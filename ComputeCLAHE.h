////////////////////////////////////////
// ComputeCLAHE.h
// Compute CLAHE using Compute Shaders and multi-threading
////////////////////////////////////////

#pragma once

#include "core.h"

class ComputeCLAHE {
private:

	// compute shaders
	GLuint _minMaxShader, _minMaxMaskedShader, _LUTshader, _LUTMaskedshader;
	GLuint _histShader, _histMaskedShader;
	GLuint _excessShader, _clipShaderPass1, _clipShaderPass2;
	GLuint _lerpShader, _lerpFocusedShader, _lerpMaskedShader;

	// volume data 
	GLuint _volumeTexture, _maskTexture;
	glm::ivec3 _volDims;
	unsigned int _numOutGrayVals;
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
	int _numOrgans = 4;

	// CLAHE Compute Shader Functions
	uint32_t computeLUT(glm::uvec3 volDims, uint32_t* minMax, bool useMask=false, glm::uvec3 offset = glm::uvec3(0));
	void computeMaskedLUT(glm::uvec3 volDims, uint32_t* min, uint32_t* max, uint32_t* pixelCount);

	void computeHist(glm::uvec3 volDims, glm::uvec3 numSB, bool useMask = false, glm::uvec3 offset = glm::uvec3(0));
	void computeMaskedHist(glm::uvec3 volDims);

	void computeClipHist(glm::uvec3 volDims, glm::uvec3 numSB, float clipLimit, uint32_t* minMax, int numPixels = -1, bool useMask = false);
	void computeMaskedClipHist(glm::uvec3 volDims, float clipLimit, uint32_t* min, uint32_t* max, uint32_t* numPixels);

	GLuint computeLerp(GLuint& maskedVersion, glm::uvec3 volDims, glm::uvec3 numSB, bool useMask=false, glm::uvec3 offset = glm::uvec3(0));
	GLuint computeFocusedLerp(glm::uvec3 volDims, glm::uvec3 numSB, glm::uvec3 minVal, glm::vec3 maxVal);
	GLuint computeMaskedLerp(glm::uvec3 volDims);

public:
	ComputeCLAHE() {};
	ComputeCLAHE(GLuint volumeTexture, glm::ivec3 volDims, unsigned int finalGrayVals, unsigned int inGrayVals);
	~ComputeCLAHE();

	void Init(GLuint volumeTexture, GLuint maskTexture, glm::ivec3 volDims, unsigned int finalGrayVals, unsigned int inGrayVals);

	// CLAHE Methods
	GLuint Compute3D_CLAHE(GLuint& maskedVersion, glm::uvec3 numSB, float clipLimit, bool useMask=false);
	GLuint ComputeFocused3D_CLAHE(glm::ivec3 min, glm::ivec3 max, float clipLimit);
	GLuint ComputeMasked3D_CLAHE(float clipLimit);

	// Change parameters for Focused CLAHE
	bool ChangePixelsPerSB(bool decrease);
};