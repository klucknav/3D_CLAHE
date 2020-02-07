////////////////////////////////////////
// ImageLoader.h
////////////////////////////////////////

#pragma once

#include <gl/glew.h>
#include <glm/glm.hpp>

#include <memory>
#include <string>


// Win32 LoadImage macro
#ifdef LoadImage
#undef LoadImage
#endif

class ImageLoader {
public:
	static GLuint LoadImage(const std::string& imagePath, glm::vec3& size);
	static GLuint LoadVolume(const std::string& folder, glm::vec3& size);
	//static void LoadMask(const std::string& folder, const GLuint& texture);
};