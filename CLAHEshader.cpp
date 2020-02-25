////////////////////////////////////////
// CLAHEshader.cpp
////////////////////////////////////////

#include "CLAHEshader.h"
#include "Shader.h"


////////////////////////////////////////////////////////////////////////////////
// Constructors/Destructors

CLAHEshader::CLAHEshader(GLuint volumeTexture, glm::vec3 volDims, unsigned int numFinalGrayVals) {
	_minMaxShader = LoadComputeShader("minMax.comp");
	_LUTshader = LoadComputeShader("LUT.comp");
	_histShader = LoadComputeShader("hist.comp");
	//_lerpShader = LoadComputeShader("lerp.comp");

	// data used in the compute shaders 
	_volumeTexture = volumeTexture;
	_volDims = volDims;
	_numFinalGrayVals = numFinalGrayVals;
	_data[0] = _numFinalGrayVals;
	_data[1] = 0;
}

CLAHEshader::~CLAHEshader() {
	glDeleteProgram(_minMaxShader);
	glDeleteProgram(_LUTshader);
	glDeleteProgram(_histShader);
	//glDeleteProgram(_lerpShader);
}

////////////////////////////////////////////////////////////////////////////////
// Compute Shader Functions

void CLAHEshader::ComputeMinMax() {

	// buffer to store the min/max
	glGenBuffers(1, &_tempBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _tempBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 2 * sizeof(uint32_t), _data, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Set up Compute Shader 
	glUseProgram(_minMaxShader);
	glBindImageTexture(0, _volumeTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R16);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _tempBuffer);
	glUniform1ui(glGetUniformLocation(_minMaxShader, "NUM_BINS"), _numFinalGrayVals);

	glDispatchCompute(	(GLuint)((_volDims.x + 3) / 4),
						(GLuint)((_volDims.y + 3) / 4),
						(GLuint)((_volDims.z + 3) / 4));
	// make sure writting to the image is finished before reading 
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glUseProgram(0);

}

void CLAHEshader::ComputeLUT() {

	// buffer to store the LUT
	glGenBuffers(1, &_LUTbuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _LUTbuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, _numFinalGrayVals * sizeof(uint32_t), nullptr, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Set up Compute Shader 
	glUseProgram(_LUTshader);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _tempBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _LUTbuffer);
	glUniform1ui(glGetUniformLocation(_LUTshader, "NUM_OUT_BINS"), _numFinalGrayVals);

	glDispatchCompute((GLuint)(_numFinalGrayVals / 64), 1, 1);

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	glUseProgram(0);

}

void CLAHEshader::ComputeHist() {


	// Set up Compute Shader 
	glUseProgram(_histShader);
	glBindImageTexture(0, _volumeTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R16);

	glDispatchCompute(	(GLuint)((_volDims.x + 3) / 4),
						(GLuint)((_volDims.y + 3) / 4),
						(GLuint)((_volDims.z + 3) / 4));
	// make sure writting to the image is finished before reading 
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glUseProgram(0);
}

////////////////////////////////////////////////////////////////////////////////