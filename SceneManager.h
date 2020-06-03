////////////////////////////////////////
// SceneManager.h
////////////////////////////////////////

#pragma once

#include "core.h"
#include "Cube.h"
#include "Camera.h"

class SceneManager {

private:
	// Window Variables
	static int _windowWidth;
	static int _windowHeight;
	static GLFWwindow* _window;

	// Scene Variables 
	static Camera* _camera;
	// 3D Volume
	static Cube* _dicomCube;
	static GLuint _dicomVolumeTexture;
	static GLuint _dicomMaskTexture;
	// CLAHE Textures
	static GLuint _3D_CLAHE;
	static GLuint _FocusedCLAHE;
	static GLuint _MaskedCLAHE;
	// Volume Shader 
	static GLuint _volumeShader;

	// Helper Functions
	static void printMat(glm::mat4);
	static void printVec(glm::vec3);
	static void updateVolume();

public:
	// called from main
	static int CreateWindow(const char* title, int window_width, int window_height);
	static void InitScene();
	static void ClearScene();
	static void Update();
	static void Draw();
	static bool WindowOpen();

	// GLFW callbacks
	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void ResizeCallback(GLFWwindow* window, int width, int height);
	static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	static void CursorPositionCallback(GLFWwindow* window, double currX, double currY);

	// Getters
	static int GetWindowWidth()			{ return _windowWidth; }
	static int GetWindowHeight()		{ return _windowHeight; }
};