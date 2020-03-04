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
	GLuint _lerpShader;

	// volume data 
	GLuint _volumeTexture;
	glm::vec3 _volDims;
	unsigned int _numFinalGrayVals;
	unsigned int _numInGrayVals;

	// CLAHE Data
	glm::vec3 _numSB = glm::vec3(4, 4, 2);
	float _clipLimit = 0.85f;

	// calculated data
	uint32_t _globalMinMax[2];
	GLuint _newVolumeBuffer;
	GLuint _newVolumeTexture;

	// buffers 
	GLuint _globalMinMaxBuffer, _LUTbuffer, _histBuffer;
	GLuint _histMaxBuffer, _excessBuffer;
	GLuint _layer = 1;

public:
	ComputeCLAHE(GLuint volumeTexture, glm::vec3 volDims, unsigned int numFinalGrayVals, unsigned int numInGrayVals);
	~ComputeCLAHE();

	GLuint Compute3D_CLAHE(glm::uvec3 numSB);
	GLuint ComputeFocused3D_CLAHE(glm::uvec3 min, glm::uvec3 max);

	void ComputeMinMax();
	void ComputeLUT();
	void ComputeHist();
	void ComputeClipHist();
	void ComputeLerp();

	GLuint GetNewTexture() { return _newVolumeTexture; }
};