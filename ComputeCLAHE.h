////////////////////////////////////////
// ComputeCLAHE.h
// Compute CLAHE using Compute Shaders and multi-threading
////////////////////////////////////////

#pragma once

#include "core.h"

class ComputeCLAHE {
private:

	// CLAHE and Focused CLAHE Compute Shaders
	GLuint _minMaxShader, _LUTshader; 
	GLuint _histShader;
	GLuint _excessShader, _clipShaderPass1, _clipShaderPass2;
	GLuint _lerpShader, _lerpShader_Focused;
	// Masked CLAHE Compute Shaders
	GLuint _minMaxShader_Masked, _LUTShader_Masked;
	GLuint _histShader_Masked;
	GLuint _excessShader_Masked, _clipShaderPass1_Masked, _clipShaderPass2_Masked;
	GLuint _lerpShader_Masked;

	// DICON Volume Data 
	GLuint _volumeTexture, _maskTexture;
	unsigned int _numOutGrayVals, _numInGrayVals;
	glm::ivec3 _volDims;

	// CLAHE Buffers and Data
	GLuint _LUTbuffer, _histBuffer, _histMaxBuffer;
	GLuint _layer = 1;

	// Focused CLAHE Parameters
	glm::ivec3 _pixelRatio = glm::ivec3(100, 100, 50);
	glm::ivec3 _minPixels = glm::ivec3(25, 25, 20);

	// Masked CLAHE Parameters
	int _numOrgans = 4;

	// CLAHE Compute Shader Functions
	void computeLUT(glm::uvec3 volDims, uint32_t* minMax, bool useLUT,  glm::uvec3 offset = glm::uvec3(0));
	void computeLUT_Masked(glm::uvec3 volDims, uint32_t* min, uint32_t* max, uint32_t* pixelCount);

	void computeHist(glm::uvec3 volDims, glm::uvec3 numSB, bool useLUT, glm::uvec3 offset = glm::uvec3(0));
	void computeHist_Masked(glm::uvec3 volDims, bool useLUT);

	void computeClipHist(glm::uvec3 volDims, glm::uvec3 numSB, float clipLimit, uint32_t* minMax, int numPixels = -1);
	void computeClipHist_Masked(glm::uvec3 volDims, float clipLimit, uint32_t* min, uint32_t* max, uint32_t* numPixels);

	GLuint computeLerp(glm::uvec3 volDims, glm::uvec3 numSB, bool useLUT, glm::uvec3 offset = glm::uvec3(0));
	GLuint computeLerp_Focused(glm::uvec3 volDims, glm::uvec3 numSB, glm::uvec3 minVal, glm::vec3 maxVal, bool useLUT);
	GLuint computeLerp_Masked(glm::uvec3 volDims, bool useLUT);

public:
	ComputeCLAHE() {};
	ComputeCLAHE(GLuint volumeTexture, GLuint maskTexture, glm::ivec3 volDims, unsigned int finalGrayVals, 
				unsigned int inGrayVals, unsigned int numOrgans);
	~ComputeCLAHE();

	void Init(GLuint volumeTexture, GLuint maskTexture, glm::ivec3 volDims, unsigned int finalGrayVals, 
				unsigned int inGrayVals, unsigned int numOrgans);

	// CLAHE Methods
	GLuint Compute3D_CLAHE(glm::uvec3 numSB, float clipLimit);
	GLuint ComputeFocused3D_CLAHE(glm::ivec3 min, glm::ivec3 max, float clipLimit);
	GLuint ComputeMasked3D_CLAHE(float clipLimit);

	// Change parameters for Focused CLAHE
	bool ChangePixelsPerSB(bool decrease);
};