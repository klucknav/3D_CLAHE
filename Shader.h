////////////////////////////////////////
// Shader.h
////////////////////////////////////////

#pragma once

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#include <stdio.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>


enum class ShaderType { VERTEX, FRAGMENT, COMPUTE };

GLuint LoadSingleShader(const char* shaderFilePath, ShaderType type);

GLuint LoadComputeShader(const char* computerShaderPath);

GLuint LoadShaders(const char* vertexShaderPath, const char* fragmentShaderPath);