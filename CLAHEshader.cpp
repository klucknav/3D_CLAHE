////////////////////////////////////////
// CLAHEshader.cpp
////////////////////////////////////////

#include "CLAHEshader.h"
#include "Shader.h"


////////////////////////////////////////////////////////////////////////////////
// Constructors/Destructors

CLAHEshader::CLAHEshader(GLuint volumeTexture, glm::vec3 volDims, unsigned int finalGrayVals, unsigned int inGrayVals) {
	
	// Load Shaders 
	_minMaxShader = LoadComputeShader("minMax.comp");
	_LUTshader = LoadComputeShader("LUT.comp");
	_histShader = LoadComputeShader("hist.comp");
	_clipShader = LoadComputeShader("clipHist.comp");
	_mapShader = LoadComputeShader("mapHist.comp");
	//_lerpShader = LoadComputeShader("lerp.comp");

	// Volume Data 
	_volumeTexture = volumeTexture;
	_volDims = volDims;
	_numFinalGrayVals = finalGrayVals;
	_numInGrayVals = inGrayVals;

	// Data used in the compute shaders 
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
	glBindImageTexture(0, _volumeTexture, 0, GL_TRUE, _layer, GL_READ_ONLY, GL_R16UI);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _tempBuffer);
	glUniform1ui(glGetUniformLocation(_minMaxShader, "NUM_BINS"), _numInGrayVals);

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

	glm::ivec3 numSB = glm::ivec3(4, 4, 2);

	// Buffer to store the Histograms
	uint32_t histSize = _numFinalGrayVals * numSB.x * numSB.y * numSB.z;
	glGenBuffers(1, &_histBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _histBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, histSize * sizeof(uint32_t), nullptr, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);


	glGenBuffers(1, &_histMaxBuffer);
	uint32_t maxValSize = numSB.x * numSB.y * numSB.z;
	uint32_t* _histMax = new uint32_t[numSB.x * numSB.y * numSB.z];
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

	glDispatchCompute(	(GLuint)((_volDims.x + 3) / 4),
						(GLuint)((_volDims.y + 3) / 4),
						(GLuint)((_volDims.z + 3) / 4));

	// make sure writting to the image is finished before reading 
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glUseProgram(0);
}

void CLAHEshader::ComputeClipHist() {

	std::cerr << "Clip Histogram\n";

	float clipLimit = 0.85f;
	glm::vec3 numSB = glm::vec3(4, 4, 2);
	glm::vec3 sizeSB = glm::ivec3( _volDims / numSB);
	uint32_t histSize = _numFinalGrayVals * numSB.x * numSB.y * numSB.z;


	glGenBuffers(1, &_excessBuffer);
	uint32_t excessSize = numSB.x * numSB.y * numSB.z;
	uint32_t* excess = new uint32_t[numSB.x * numSB.y * numSB.z];
	memset(excess, 0, excessSize * sizeof(uint32_t));

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, _excessBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, excessSize* sizeof(uint32_t), excess, GL_STREAM_READ);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Set up Compute Shader 
	glUseProgram(_clipShader);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _histBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _histMaxBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _excessBuffer);
	glUniform3i(glGetUniformLocation(_clipShader, "numSB"), numSB.x, numSB.y, numSB.z);
	glUniform1f(glGetUniformLocation(_clipShader, "clipLimit"), clipLimit);

	glDispatchCompute( (GLuint)(histSize / 64), 1, 1 );

	// make sure writting to the image is finished before reading 
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	glUseProgram(0);
	
}

////////////////////////////////////////////////////////////////////////////////