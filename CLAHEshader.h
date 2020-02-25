////////////////////////////////////////
// CLAHEshader.h
////////////////////////////////////////

#pragma once

#include "core.h"

class CLAHEshader {
private:

	// compute shaders
	GLuint _minMaxShader;
	GLuint _LUTshader;
	GLuint _histShader;
	//GLuint _lerpShader;

	// data 
	GLuint _volumeTexture;
	glm::vec3 _volDims;
	unsigned int _numFinalGrayVals;
	uint32_t _data[2];

	// buffers 
	GLuint _tempBuffer, _LUTbuffer;
	GLuint _layer = 1;

public:
	CLAHEshader(GLuint volumeTexture, glm::vec3 volDims, unsigned int numFinalGrayVals);
	~CLAHEshader();

	void ComputeMinMax();
	void ComputeLUT();
	void ComputeHist();
	void ComputeLerp();
};