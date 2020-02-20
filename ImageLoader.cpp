////////////////////////////////////////
// ImageLoader.cpp
////////////////////////////////////////

#define THREAD_COUNT 6

#ifdef WIN32
#define NOMINMAX
#include <Shlwapi.h>
#endif

#include <dcmtk/dcmimgle/dcmimage.h>
#include <dcmtk/dcmdata/dctk.h>

#include <thread>
#include <algorithm>

#include "ImageLoader.h"

using namespace std;


////////////////////////////////////////////////////////////////////////////////
// File Helpers 

string GetExt(const string& path) {
	size_t k = path.rfind('.');
	if (k == string::npos) return "";
	return path.substr((int)k + 1);
}
string GetName(const string& path) {
	char const* str = path.c_str();

	int f = 0;
	int l = 0;
	for (int i = 0; i < path.length(); i++) {
		if (str[i] == '\\' || str[i] == '/')
			f = i + 1;
		else if (str[i] == '.')
			l = i;
	}

	return path.substr(f, l - f);
}
string GetFullPath(const string& str) {
#ifdef WIN32
	char buf[MAX_PATH];
	if (GetFullPathName(str.c_str(), 256, buf, nullptr) == 0) {
		printf("Failed to get full file path of %s (%d)", str.c_str(), GetLastError());
		return str;
	}
	return string(buf);
#endif
}
void GetFiles(const string& path, vector<string>& files) {
#ifdef WIN32
	string d = path + "\\*";

	WIN32_FIND_DATAA ffd;
	HANDLE hFind = FindFirstFile(d.c_str(), &ffd);
	if (hFind == INVALID_HANDLE_VALUE) {
		assert(false);
		return;
	}

	do {
		if (ffd.cFileName[0] == L'.') continue;

		string c = path + "\\" + ffd.cFileName;

		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			// file is a directory
		}
		else {
			string ext = GetExt(c);
			if (ext == "dcm" || ext == "raw" || ext == "png")
				files.push_back(GetFullPath(c));
		}
	} while (FindNextFileA(hFind, &ffd) != 0);

	FindClose(hFind);
#endif
}

////////////////////////////////////////////////////////////////////////////////
// Texture Helpers 

GLuint InitTexture2D(unsigned int width, unsigned int height, 
		GLenum internalFormat, GLenum format, GLenum type, GLenum filter, void* data) {

	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

	glBindTexture(GL_TEXTURE_2D, 0);

	return textureID;
}

GLuint InitTexture3D(unsigned int width, unsigned int height, unsigned int depth,
	GLenum internalFormat, GLenum format, GLenum type, GLenum filter, void* data) {

	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_3D, textureID);
	glTexImage3D(GL_TEXTURE_3D, 0, internalFormat, width, height, depth, 0, format, type, data);

	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
							   
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, filter);

	glBindTexture(GL_TEXTURE_3D, 0);

	return textureID;
}

////////////////////////////////////////////////////////////////////////////////
// Dicom Readers

struct Slice {
	DicomImage* image;
	double location;
};

void ReadDicomSlice(DicomImage*& img, double& x, const string& file, double& spacingX, double& spacingY, double& thickness) {
	OFCondition cnd;

	DcmFileFormat fileFormat;
	assert((cnd = fileFormat.loadFile(file.c_str())).good());
	DcmDataset* dataset = fileFormat.getDataset();

	double sx;
	double sy;
	double th;
	cnd = dataset->findAndGetFloat64(DCM_PixelSpacing, sx, 0);
	cnd = dataset->findAndGetFloat64(DCM_PixelSpacing, sy, 1);
	cnd = dataset->findAndGetFloat64(DCM_SliceThickness, th, 0);

	spacingX = std::max(sx, spacingX);
	spacingY = std::max(sy, spacingY);
	thickness = std::max(th, thickness);

	cnd = dataset->findAndGetFloat64(DCM_SliceLocation, x, 0);

	img = new DicomImage(file.c_str());
	assert(img != NULL);
	assert(img->getStatus() == EIS_Normal);
}

void ReadDicomImage(DicomImage* img, uint16_t* slice, int w, int h) {
	img->setMinMaxWindow();
	uint16_t* pixelData = (uint16_t*)img->getOutputData(16);
	memcpy(slice, pixelData, sizeof(uint16_t) * w * h);
}

void ReadDicomImages(uint16_t* data, vector<Slice>& images, int start, int end, int w, int h) {

	for (int sliceID = start; sliceID < end; sliceID++) {
		if (sliceID >= (int)images.size()) 
			break;
		//printf("slice: %d, adding: %d\n", sliceID, (sliceID * w * h));
		ReadDicomImage(images[sliceID].image, data + sliceID * w * h, w, h);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Dicom Loaders

GLuint ImageLoader::loadDicomImage() {

	printf("Loading Dicom file: %s\n", _path.c_str());

	// Get information
	DicomImage* image = nullptr;
	double x = 0.0;
	double spacingX = 0.0;
	double spacingY = 0.0;
	double thickness = 0.0;
	ReadDicomSlice(image, x, _path, spacingX, spacingY, thickness);

	unsigned int w = image->getWidth();
	unsigned int h = image->getHeight();
	unsigned int d = 1;

	_imgDims.x = w;
	_imgDims.y = h;
	_imgDims.z = d;

	// volume size in meters
	_size.x = .001f * (float)spacingX * w;
	_size.y = .001f * (float)spacingY * h;
	_size.z = .001f * (float)thickness;

	_imageData = new uint16_t[w * h * d];
	memset(_imageData, 0xFF, w * h * d * sizeof(uint16_t));
	ReadDicomImage(image, _imageData, w, h);

	// get the raw data 
	//_dataTest = (uint16_t*)_imageData; 
	//_dataTest = (uint16_t*)image->getOutputData(16);
	//_dataTest = (uint16_t*)image->getInterData();

	// Get the Min/Max values for the Dicom Image
	int mode = 0;	// mode = used pixel values vs 1 = possible pixelvalues
	image->getMinMaxValues(_minPixelVal, _maxPixelVal, mode);

	GLuint tex = InitTexture2D(w, h, GL_R16, GL_RED, GL_UNSIGNED_SHORT, GL_LINEAR, _imageData);
	//delete[] data;
	cerr << "Dicom loaded: (" << tex << ")\n\n";
	return tex;
}

GLuint ImageLoader::loadDicomVolume(const vector<string>& files) {

	std::cerr << "Loading DICOM Folder\n";

	vector<Slice> images;

	// Get information
	double spacingX = 0.0;
	double spacingY = 0.0;
	double thickness = 0.0;

	for (unsigned int i = 0; i < (int)files.size(); i++) {
		DicomImage* img;
		double x;
		ReadDicomSlice(img, x, files[i], spacingX, spacingY, thickness);
		images.push_back({ img, x });
	}

	std::sort(images.begin(), images.end(), [](const Slice& a, const Slice& b) {
		return a.location < b.location;
		});

	unsigned int w = images[0].image->getWidth();
	unsigned int h = images[0].image->getHeight();
	unsigned int d = (unsigned int)images.size();

	_imgDims.x = w;
	_imgDims.y = h;
	_imgDims.z = d;

	// volume size in meters
	_size.x = .001f * (float)spacingX * w;
	_size.y = .001f * (float)spacingY * h;
	_size.z = .001f * (float)thickness * images.size();

	printf("%fm x %fm x %fm\n", _size.x, _size.y, _size.z);

	_imageData = new uint16_t[w * h * d];
	memset(_imageData, 0xFFFF, w * h * d * sizeof(uint16_t));

	if (THREAD_COUNT > 1) {
		printf("reading %d slices\n", d);
		vector<thread> threads;
		int s = ((int)images.size() + THREAD_COUNT - 1) / THREAD_COUNT;

		// each thread processes s slices 
		for (int i = 0; i < (int)images.size(); i += s) {
			threads.push_back(thread(ReadDicomImages, _imageData, images, i, i + s, w, h));
		}

		for (int i = 0; i < (int)threads.size(); i++) {
			threads[i].join();
		}
	}
	else {
		ReadDicomImages(_imageData, images, 0, (int)images.size(), w, h);
	}

	GLuint tex = InitTexture3D(w, h, d, GL_R16, GL_RED, GL_UNSIGNED_SHORT, GL_LINEAR, _imageData);
	printf("Dicom Volume loaded: (%d)\n\n", tex);
	return tex;
}

////////////////////////////////////////////////////////////////////////////////
// Image/Volume Loaders 

GLuint ImageLoader::loadImage() {
#ifdef WIN32
	if (!PathFileExists(_path.c_str())) {
#endif
		printf("%s Does not exist!\n", _path.c_str());
		return 0;
	}

	string ext = GetExt(_path.c_str());
	if (ext == "dcm")
		return loadDicomImage();
	else
		return 0;
}

GLuint ImageLoader::loadVolume() {
#ifdef WIN32
	if (!PathFileExists(_path.c_str())) {
#endif
		printf("%s Does not exist!\n", _path.c_str());
		return 0;
	}

	vector<string> files;
	GetFiles(_path, files);

	if (files.size() == 0) return 0;

	string ext = GetExt(files[0]);
	if (ext == "dcm")
		return loadDicomVolume(files);
	else
		return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Constructor/Destructor

ImageLoader::ImageLoader(string& path, bool isImg) {

	_path = path;

	if (isImg) {
		cerr << "Load Image\n";
		_textureID = loadImage();
	}
	else {
		cerr << "Load Volume\n";
		_textureID = loadVolume();
	}
}

ImageLoader::~ImageLoader() {
	delete[] _imageData;
}

////////////////////////////////////////////////////////////////////////////////