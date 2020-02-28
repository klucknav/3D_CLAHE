////////////////////////////////////////
// CLAHEshader.h
////////////////////////////////////////

#pragma once

#include "core.h"

class CLAHEshader {
private:

	// compute shaders
	GLuint _minMaxShader, _LUTshader;
	GLuint _histShader, _excessShader, _clipShader1, _clipShader2;
	//GLuint _lerpShader;

	// volume data 
	GLuint _volumeTexture;
	glm::vec3 _volDims;
	unsigned int _numFinalGrayVals;
	unsigned int _numInGrayVals;

	// CLAHE Data
	glm::vec3 _numSB = glm::vec3(4, 4, 2);
	//unsigned int _totalNumSB = _numSB.x * _numSB.y * _numSB.z;
	float _clipLimit = 0.85f;

	// calculated data
	uint32_t _globalMinMax[2];
	//uint32_t _LUT[2];

	// buffers 
	GLuint _globalMinMaxBuffer, _LUTbuffer, _histBuffer;
	GLuint _histMaxBuffer, _excessBuffer;
	GLuint _layer = 1;

	//void mapHistogram();

public:
	CLAHEshader(GLuint volumeTexture, glm::vec3 volDims, unsigned int numFinalGrayVals, unsigned int numInGrayVals);
	~CLAHEshader();

	void ComputeMinMax();
	void ComputeLUT();
	void ComputeHist();
	void ComputeClipHist();
	void ComputeLerp();
};