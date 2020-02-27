////////////////////////////////////////
// SceneManager.h
////////////////////////////////////////

#include "SceneManager.h"
#include "Shader.h"
#include "ImageLoader.h"
#include "CLAHE.h"
#include "CLAHEshader.h"

#include <stdio.h>

// Window Variables
int SceneManager::_windowWidth;
int SceneManager::_windowHeight;
GLFWwindow* SceneManager::_window;

// Scene Variables
Camera* SceneManager::_camera;
bool SceneManager::_drawVolume;
// 2D Image
ImageLoader* _dicomImage;
GLuint SceneManager::_dicomTexture;
GLuint SceneManager::_claheDicomTexture;
GLuint SceneManager::_focusedDicomTexture;
// 3D Volume
ImageLoader* _dicomVolume;
Cube* SceneManager::_dicomCube;
GLuint SceneManager::_dicomVolumeTexture;
GLuint SceneManager::_claheDicomVolumeTexture;
GLuint SceneManager::_focusedDicomVolumeTexture;

enum class TextureMode {
	_RAW,
	_CLAHE,
	_FOCUSED
};
TextureMode _textureMode;

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

	_drawVolume = false;
	glm::vec3 size, imgDims, volDims;

	////////////////////////////////////////////////////////////////////////////
	// 2D CLAHE - Single Texture
	glGenVertexArrays(1, &_VAO);
	std::string path = std::string("C:/Users/kroth/Documents/UCSD/Grad/Thesis/clahe_2/dicom/sample.dcm");
	ImageLoader* _dicomImage = new ImageLoader(path);
	_dicomTexture = _dicomImage->GetTextureID();

	// 2D CLAHE
	/*unsigned int numCRx = 2, numCRy = 2;
	unsigned int numGrayValsFinal = 65536;
	float clipLimit = 0.85f;

	CLAHE * _test = new CLAHE(_dicomImage);

	// Regular 2D CLAHE
	_claheDicomTexture = _test->CLAHE_2D(numCRx, numCRy, numGrayValsFinal, clipLimit);

	// Focused 2D CLAHE
	/*unsigned int xMin = 200;
	unsigned int xMax = 400;
	unsigned int yMin = 200;
	unsigned int yMax = 400;
	_focusedDicomTexture = _test->Focused_CLAHE_2D(xMin, yMin, xMax, yMax, numGrayValsFinal, clipLimit);*/

	////////////////////////////////////////////////////////////////////////////
	// 3D CLAHE - Cube Volume 
	_dicomCube = new Cube();
	std::string folderPath = std::string("C:/Users/kroth/Documents/UCSD/Grad/Thesis/clahe_2/Larry");
	ImageLoader* _dicomVolume = new ImageLoader(folderPath, false);
	glm::vec3 volDim = _dicomVolume->GetImageDimensions();
	printf("Volume Dimensions:  ");
	printVec(volDim);
	_dicomVolumeTexture = _dicomVolume->GetTextureID();

	// 3D CLAHE
	CLAHE* _volumeTest = new CLAHE(_dicomVolume);
	
	//unsigned int numCRx = 2, numCRy = 2,  numCRz = 1;
	unsigned int numCRx = 4, numCRy = 4,  numCRz = 2;
	unsigned int numGrayValsFinal = 65536;
	float clipLimit = 0.85f;
	_claheDicomVolumeTexture = _volumeTest->CLAHE_3D(numCRx, numCRy, numCRz, numGrayValsFinal, clipLimit);
	/*
	// Focused 3D CLAHE
	unsigned int zMin = 50;
	unsigned int zMax = 100;
	_focusedDicomVolumeTexture = _volumeTest->Focused_CLAHE_3D(xMin, xMax, yMin, yMax, zMin, zMax, numGrayValsFinal, clipLimit);*/

	////////////////////////////////////////////////////////////////////////////
	// Using Compute Shaders

	unsigned int numFinalGrayVals = 65536;
	CLAHEshader comp = CLAHEshader(_dicomVolumeTexture, volDim, numFinalGrayVals, numFinalGrayVals);
	comp.ComputeMinMax();
	comp.ComputeLUT();
	comp.ComputeHist();
	comp.ComputeClipHist();

	////////////////////////////////////////////////////////////////////////////
	// Fake Data for Testing
	/*int w = 8;
	int h = 8;
	int d = 1;
	glm::vec3 fakeDims = glm::vec3(w, h, d);
	int fakeSize = w * h * d;
	uint16_t* fakeImage = new uint16_t[fakeSize];
	//cerr << "fakeImage: \n";
	for (int i = 0; i < fakeSize; i++) {
		fakeImage[i] = i;
		//cerr << "(" << i << ", " << fakeImage[i] << ")\n";
	}
	//cerr << endl << endl;
	CLAHE* _imageTest = new CLAHE(fakeImage, fakeDims, 0, fakeSize - 1);
	GLuint result = _imageTest->CLAHE_2D(2, 2, fakeSize, 0.85f);
	w = 8;
	h = 8;
	d = 4;
	fakeDims = glm::vec3(w, h, d);
	fakeSize = w * h * d;
	uint16_t* fakeVol = new uint16_t[fakeSize];
	//cerr << "fakeImage: \n";
	for (int i = 0; i < fakeSize; i++) {
		fakeVol[i] = i;
		//cerr << "(" << i << ", " << fakeImage[i] << ")\n";
	}
	//cerr << endl << endl;
	CLAHE* _volumeTest = new CLAHE(fakeVol, fakeDims, 0, fakeSize-1);
	result = _volumeTest->CLAHE_3D(2, 2, 2, fakeSize, 0.85f);*/
	////////////////////////////////////////////////////////////////////////////

	_textureMode = TextureMode::_RAW;
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
		switch (_textureMode) {
			case TextureMode::_RAW:
				_dicomCube->Draw(_volumeShader, _camera->GetViewProjectMtx(), _camera->GetCamPos(), _dicomVolumeTexture);
				break;
			case TextureMode::_CLAHE:
				_dicomCube->Draw(_volumeShader, _camera->GetViewProjectMtx(), _camera->GetCamPos(), _claheDicomVolumeTexture);
				break;
			case TextureMode::_FOCUSED:
				_dicomCube->Draw(_volumeShader, _camera->GetViewProjectMtx(), _camera->GetCamPos(), _focusedDicomVolumeTexture);
				break;
		}
	}

	// draw the texture 
	else {
		glUseProgram(_displayShader);
		glBindVertexArray(_VAO);
		glActiveTexture(GL_TEXTURE0);
		switch (_textureMode) {
			case TextureMode::_RAW:
				glBindTexture(GL_TEXTURE_2D, _dicomTexture);
				break;
			case TextureMode::_CLAHE:
				glBindTexture(GL_TEXTURE_2D, _claheDicomTexture);
				break;
			case TextureMode::_FOCUSED:
				glBindTexture(GL_TEXTURE_2D, _focusedDicomTexture);
				break;
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
			case GLFW_KEY_D:
				_textureMode = TextureMode::_RAW;
				break;
			case GLFW_KEY_C:
				_textureMode = TextureMode::_CLAHE;
				break;
			case GLFW_KEY_F:
				_textureMode = TextureMode::_FOCUSED;
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

	printf("(%f, %f, %f)\n", toPrint.x, toPrint.y, toPrint.z);
}

////////////////////////////////////////////////////////////////////////////////