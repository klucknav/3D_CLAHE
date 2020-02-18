////////////////////////////////////////
// SceneManager.h
////////////////////////////////////////

#include "SceneManager.h"
#include "Shader.h"
#include "ImageLoader.h"
#include "CLAHE.h"

#include <stdio.h>

// Window Variables
int SceneManager::_windowWidth;
int SceneManager::_windowHeight;
GLFWwindow* SceneManager::_window;

// Scene Variables
Camera* SceneManager::_camera;
ImageLoader* _dicomImage;
ImageLoader* _dicomVolume;
GLuint SceneManager::_dicomTexture;
GLuint _newDicomTexture;
bool _newTexture;
GLuint SceneManager::_dicomVolumeTexture;
GLuint _newDicomVolumeTexture;
Cube* SceneManager::_dicomCube;
bool SceneManager::_drawVolume;

// Shader Variables
GLuint SceneManager::_volumeShader;
GLuint SceneManager::_displayShader;
GLuint SceneManager::_VAO;

// Interaction Variables
bool LeftDown, RightDown;
int MouseX, MouseY;

////////////////////////////////////////////////////////////////////////////////
// Creating Window

int SceneManager::CreateWindow(const char* title, int width, int height) {

	// 8x antialiasing
	glfwWindowHint(GLFW_SAMPLES, 4);
	
	// Window Variables 
	_windowWidth = width;
	_windowHeight = height;

	// Create the window
	_window = glfwCreateWindow(width, height, "CLAHE", nullptr, nullptr);

	if (!_window) {
		fprintf(stderr, "Failed to open GLFW window.\n");
		fprintf(stderr, "Either GLFW is not installed or your graphics card does not support modern OpenGL.\n");
		glfwTerminate();
		return 0;
	}

	// Make the context of the window
	glfwMakeContextCurrent(_window);

	// Set swap interval to 1
	glfwSwapInterval(1);

	// Get the width and height of the framebuffer to properly resize the window
	glfwGetFramebufferSize(_window, &_windowWidth, &_windowHeight);

	// Setup callbacks
	glfwSetKeyCallback(_window, KeyCallback);
	glfwSetFramebufferSizeCallback(_window, ResizeCallback);
	glfwSetMouseButtonCallback(_window, MouseButtonCallback);
	glfwSetCursorPosCallback(_window, CursorPositionCallback);
	glfwSetWindowSizeCallback(_window, ResizeCallback);
	//ResizeCallback(_window, width, height);

	glEnable(GL_MULTISAMPLE);

	return 1;
}

bool SceneManager::WindowOpen() {
	return !glfwWindowShouldClose(_window);
}

////////////////////////////////////////////////////////////////////////////////
// Constructor/Destructor for the Scene

void SceneManager::InitScene() {

	_camera = new Camera();

	// load the shaders
	_volumeShader = LoadShaders("volume.vert", "volume.frag");
	_displayShader = LoadShaders("display.vert", "display.frag");
	
	// initialize Scene 
	_drawVolume = false;
	glm::vec3 size, imgDims, volDims;

	// 2D CLAHE - Single Texture
	glGenVertexArrays(1, &_VAO);
	std::string path = std::string("C:/Users/kroth/Documents/UCSD/Grad/Thesis/clahe_2/dicom/sample.dcm");
	ImageLoader* _dicomImage = new ImageLoader(path);
	_dicomTexture = _dicomImage->GetTextureID();

	// 2D CLAHE:
	unsigned int numCRx = 2, numCRy = 2;
	unsigned int numGrayValsFinal = 65536;
	float clipLimit = 0.85f;

	CLAHE * _test = new CLAHE(_dicomImage);
	GLuint result = _test->CLAHE_2D(numCRx, numCRy, numGrayValsFinal, clipLimit);
	if (result > 0) {
		_newDicomTexture = result;
	}

	// 3D CLAHE - Cube Volume 
	_dicomCube = new Cube();
	std::string folderPath = std::string("C:/Users/kroth/Documents/UCSD/Grad/Thesis/clahe_2/Larry");
	ImageLoader* _dicomVolume = new ImageLoader(folderPath, false);
	_dicomVolumeTexture = _dicomVolume->GetTextureID();

	// 3D CLAHE
	CLAHE* _volumeTest = new CLAHE(_dicomVolume);
	//CLAHE* _volumeTest = new CLAHE(_dicomImage);

	unsigned int numCRz = 1;
	result = _volumeTest->CLAHE_3D(numCRx, numCRy, numCRz, numGrayValsFinal, clipLimit);
	if (result > 0) {
		_newDicomVolumeTexture = result;
	}

	////////////////////////////////////////////////////////////////////////////
	// Fake Data for Testing
	//int w = 8;
	//int h = 8;
	//int d = 1;
	//glm::vec3 fakeDims = glm::vec3(w, h, d);
	//int fakeSize = w * h * d;
	//uint16_t* fakeImage = new uint16_t[fakeSize];
	////cerr << "fakeImage: \n";
	//for (unsigned int i = 0; i < fakeSize; i++) {
	//	fakeImage[i] = i;
	//	//cerr << "(" << i << ", " << fakeImage[i] << ")\n";
	//}
	////cerr << endl << endl;
	//CLAHE* _imageTest = new CLAHE(fakeImage, fakeDims, 0, fakeSize - 1);
	//result = _imageTest->CLAHE_2D(2, 2, fakeSize, 0.85);
	//w = 8;
	//h = 8;
	//d = 2;
	//fakeDims = glm::vec3(w, h, d);
	//fakeSize = w * h * d;
	//uint16_t* fakeVol = new uint16_t[fakeSize];
	////cerr << "fakeImage: \n";
	//for (unsigned int i = 0; i < fakeSize; i++) {
	//	fakeVol[i] = i;
	//	//cerr << "(" << i << ", " << fakeImage[i] << ")\n";
	//}
	////cerr << endl << endl;
	//CLAHE* _volumeTest = new CLAHE(fakeVol, fakeDims, 0, fakeSize-1);
	//result = _volumeTest->CLAHE_3D(2, 2, 2, fakeSize, 0.85);
	////////////////////////////////////////////////////////////////////////////

	_newTexture = false;
}

void SceneManager::ClearScene() {

	glDeleteProgram(_displayShader);
	glDeleteProgram(_volumeShader);

	glDeleteVertexArrays(1, &_VAO);

	delete _camera;
	delete _dicomCube;
	delete _dicomImage;
	//delete _dicomVolume;

	glfwDestroyWindow(_window);
}

////////////////////////////////////////////////////////////////////////////////
// Draw and Update

void SceneManager::Update() {

	_camera->Update();

	// Gets events, including input such as keyboard and mouse or window resizing
	glfwPollEvents();
}

void SceneManager::Draw() {
	// Clear the color and depth buffers
	glClearColor(0.52f, 0.81f, 0.92f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// draw the volume
	if (_drawVolume) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		if (_newTexture) {
			_dicomCube->Draw(_volumeShader, _camera->GetViewProjectMtx(), _camera->GetCamPos(), _newDicomVolumeTexture);
		}
		else {
			_dicomCube->Draw(_volumeShader, _camera->GetViewProjectMtx(), _camera->GetCamPos(), _dicomVolumeTexture);
		}
	}

	// draw the texture 
	else {
		glUseProgram(_displayShader);
		glBindVertexArray(_VAO);
		glActiveTexture(GL_TEXTURE0);
		if (_newTexture) {
			glBindTexture(GL_TEXTURE_2D, _newDicomTexture);
		}
		else {
			glBindTexture(GL_TEXTURE_2D, _dicomTexture);
		}
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}

	// Swap buffers
	glfwSwapBuffers(_window);
}

////////////////////////////////////////////////////////////////////////////////
// Callbacks 

void SceneManager::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {

	if (action == GLFW_PRESS) {
		switch (key) {
			case GLFW_KEY_ESCAPE:
				// Close the window. This causes the program to also terminate.
				glfwSetWindowShouldClose(window, GL_TRUE);
				break;
			case GLFW_KEY_T:
				// swap between the cube and the flat image
				_drawVolume = !_drawVolume;
				break;
			case GLFW_KEY_R:
				// reset the camera view 
				_camera->Reset();
				break;
			case GLFW_KEY_N:
				_newTexture = !_newTexture;
				break;
		}
	}
}

void SceneManager::ResizeCallback(GLFWwindow* window, int width, int height) {

	// Update Window Variables
	_windowWidth = width;
	_windowHeight = height;

	// Set the viewport size
	glViewport(0, 0, width, height);

	// Set the View and Projection Matrices
	_camera->SetAspect(float(width) / float(height));
}

void SceneManager::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		LeftDown = (action == GLFW_PRESS);
	}
	if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		RightDown = (action == GLFW_PRESS);
	}
}

void SceneManager::CursorPositionCallback(GLFWwindow* window, double currX, double currY) {

	int maxDelta = 100;
	int dx = glm::clamp((int)currX - MouseX, -maxDelta, maxDelta);
	int dy = glm::clamp(-((int)currY - MouseY), -maxDelta, maxDelta);

	MouseX = (int)currX;
	MouseY = (int)currY;

	// Move camera
	if (LeftDown) {
		_camera->UpdateAngles(dx, dy);
	}
	if (RightDown) {
		_camera->UpdateDistance(dx);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Helper Functions

void SceneManager::printMat(glm::mat4 toPrint) {
	for (int row = 0; row < 4; row++)
	{
		for (int col = 0; col < 4; col++)
		{
			std::cerr << toPrint[col][row] << " : ";
		}
		std::cerr << std::endl;
	}
	std::cerr << std::endl;
}

void SceneManager::printVec(glm::vec3 toPrint) {
	for (int i = 0; i < 3; i++) {
		std::cerr << toPrint[i] << " : ";
	}
	std::cerr << std::endl;
}

////////////////////////////////////////////////////////////////////////////////