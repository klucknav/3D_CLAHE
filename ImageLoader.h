////////////////////////////////////////
// ImageLoader.h
////////////////////////////////////////

#pragma once

#include <gl/glew.h>
#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>


// Win32 LoadImage macro
#ifdef LoadImage
#undef LoadImage
#endif

using namespace std;

class ImageLoader {

private:	
	
	// Image Properties 
	string _path, _maskPath;
	uint16_t* _imageData; 
	uint16_t* _dataTest;
	glm::vec3 _size;
	glm::uvec3 _imgDims;
	double _minPixelVal, _maxPixelVal;

	// texture ID
	GLuint _textureID, _maskID;

	// Dicom Loaders
	GLuint loadDicomImage();
	GLuint loadDicomVolume(const vector<string>& files);

	// Image/Volume Loaders 
	GLuint loadImage();
	GLuint loadVolume();
	GLuint loadMask();

public:

	ImageLoader(string& path, string&maskpath= string(""), bool isImg=true);
	~ImageLoader();

	// Getters
	GLuint GetTextureID()			{ return _textureID; }
	GLuint GetMaskID()				{ return _maskID; }
	glm::vec3 GetSize()				{ return _size; }
	glm::vec3 GetImageDimensions()	{ return _imgDims; }
	uint16_t* GetImageData()		{ return _imageData; }
	double GetMinPixelValue()		{ return _minPixelVal; }
	double GetMaxPixelValue()		{ return _maxPixelVal; }
};