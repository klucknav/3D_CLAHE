////////////////////////////////////////
// CLAHEshader.h
////////////////////////////////////////

#pragma once

#include "core.h"

class CLAHEshader {
private:

	// compute shaders
	GLuint _minMaxShader, _LUTshader;
	GLuint _histShader, _clipShader, _mapShader;
	//GLuint _lerpShader;

	// volume data 
	GLuint _volumeTexture;
	glm::vec3 _volDims;
	unsigned int _numFinalGrayVals;
	unsigned int _numInGrayVals;

	// calculated data
	uint32_t _data[2];
	//uint32_t _LUT[2];

	// buffers 
	GLuint _tempBuffer, _LUTbuffer, _histBuffer;
	GLuint _histMaxBuffer, _excessBuffer;
	GLuint _layer = 1;

public:
	CLAHEshader(GLuint volumeTexture, glm::vec3 volDims, unsigned int numFinalGrayVals, unsigned int numInGrayVals);
	~CLAHEshader();

	void ComputeMinMax();
	void ComputeLUT();
	void ComputeHist();
	void ComputeClipHist();
	void ComputeMapHist();
	void ComputeLerp();
};