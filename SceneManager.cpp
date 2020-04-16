////////////////////////////////////////
// SceneManager.h
////////////////////////////////////////

#include "SceneManager.h"
#include "Shader.h"
#include "ImageLoader.h"
#include "CLAHE.h"
#include "ComputeCLAHE.h"

#include <stdio.h>
#include <chrono>

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
GLuint _testVolume, _maskTexture, _claheFullMasked, _claheOnlyMask;
GLuint _currTexture;
bool _useMask = false;

ComputeCLAHE comp;
glm::uvec3 min3D = glm::uvec3(200, 200, 40);
glm::uvec3 max3D = glm::uvec3(400, 400, 90);
float clipLimit3D = 0.85f;

enum class TextureMode {
	_RAW,
	_CLAHE,
	_FOCUSED
};
TextureMode _textureMode;

enum class InteractionMode {
	_MOVE, 
	_CHANGE
};
InteractionMode _interactionMode;

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

	_drawVolume = true;
	glm::vec3 size, imgDims, volDims;

	////////////////////////////////////////////////////////////////////////////
	// 2D CLAHE - Single Texture
	glGenVertexArrays(1, &_VAO);
	std::string path = std::string("C:/Users/kroth/Documents/UCSD/Grad/Thesis/clahe_2/dicom/sample.dcm");
	ImageLoader* _dicomImage = new ImageLoader(path);
	_dicomTexture = _dicomImage->GetTextureID();

	// 2D CLAHE
	/*glm::uvec2 numCR = glm::uvec2(2, 2);
	//glm::uvec2 numCR = glm::uvec2(4, 4);
	unsigned int numGrayValsFinal2D = 65536;
	float clipLimit2D = 0.85f;

	CLAHE * _test = new CLAHE(_dicomImage);

	// Regular 2D CLAHE
	_claheDicomTexture = _test->CLAHE_2D(numCR, numGrayValsFinal2D, clipLimit2D);

	// Focused 2D CLAHE
	glm::uvec2 min2D = glm::uvec2(200, 200);
	glm::uvec2 max2D = glm::uvec2(400, 400);
	_focusedDicomTexture = _test->Focused_CLAHE_2D(min2D, max2D, numGrayValsFinal2D, clipLimit2D);*/

	////////////////////////////////////////////////////////////////////////////
	// 3D CLAHE - Cube Volume 
	std::cerr << "\n\n3D\n";
	_dicomCube = new Cube();
	std::string folderPath = std::string("C:/Users/kroth/Documents/UCSD/Grad/Thesis/clahe_2/Larry_2017");
	std::string maskPath = std::string("C:/Users/kroth/Documents/UCSD/Grad/Thesis/clahe_2/Larry_2017/mask");
	ImageLoader* _dicomVolume = new ImageLoader(folderPath, maskPath, false);
	glm::vec3 volDim = _dicomVolume->GetImageDimensions();
	_dicomVolumeTexture = _dicomVolume->GetTextureID();
	_maskTexture = _dicomVolume->GetMaskID();

	// 3D CLAHE
	//glm::uvec3 numSB = glm::uvec3(2, 2, 1);
	glm::uvec3 numSB = glm::uvec3(4, 4, 2);
	unsigned int finalGrayVals_3D = 65536;
	unsigned int ogGrayVals_3D = 65536;
	//float clipLimit3D = 1.5f;
	
	/*CLAHE* _volumeTest = new CLAHE(_dicomVolume);

	// Regular 3D CLAHE
	_claheDicomVolumeTexture = _volumeTest->CLAHE_3D(numSB, finalGrayVals_3D, clipLimit3D);
	_testVolume = _volumeTest->CLAHE_3D(numSB, finalGrayVals_3D, clipLimit3D);
	
	// Focused 3D CLAHE
	glm::uvec3 min3D = glm::uvec3(200, 200, 40);
	glm::uvec3 max3D = glm::uvec3(400, 400, 90);
	_focusedDicomVolumeTexture = _volumeTest->Focused_CLAHE_3D(min3D, max3D, finalGrayVals_3D, clipLimit3D);
	_testVolume = _volumeTest->Focused_CLAHE_3D(min3D, max3D, finalGrayVals_3D, clipLimit3D);*/

	////////////////////////////////////////////////////////////////////////////
	// Using Compute Shaders

	comp.Init(_dicomVolumeTexture, _maskTexture, volDim, finalGrayVals_3D, ogGrayVals_3D);
	//comp = ComputeCLAHE(_dicomVolumeTexture, volDim, finalGrayVals_3D, ogGrayVals_3D);

	_claheDicomVolumeTexture = comp.Compute3D_CLAHE(_claheFullMasked, numSB, clipLimit3D);
	_claheOnlyMask = comp.Compute3D_CLAHE(_claheFullMasked, numSB, clipLimit3D, true);
	_focusedDicomVolumeTexture = comp.ComputeFocused3D_CLAHE(min3D, max3D, clipLimit3D);
	

	////////////////////////////////////////////////////////////////////////////

	_textureMode = TextureMode::_RAW;
	_interactionMode = InteractionMode::_MOVE;
	_currTexture = _dicomVolumeTexture;	// raw dicom
}

void SceneManager::ClearScene() {

	glDeleteProgram(_displayShader);
	glDeleteProgram(_volumeShader);

	glDeleteVertexArrays(1, &_VAO);

	delete _camera;
	delete _dicomCube;

	// ImageLoaders
	delete _dicomImage;
	delete _dicomVolume;

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
		_dicomCube->Draw(_volumeShader, _camera->GetViewProjectMtx(), _camera->GetCamPos(), _currTexture, _maskTexture, _useMask);
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

	glm::uvec3 step = glm::uvec3(20, 20, 10);
	//float clip = 0.95f;
	GLuint newTexture = 0;

	if (action == GLFW_PRESS) {
		switch (key) {

			// Close the window
			case GLFW_KEY_ESCAPE:
				glfwSetWindowShouldClose(window, GL_TRUE);
				break;
				
			// reset the camera view 
			case GLFW_KEY_R:
				_camera->Reset();
				break;

			// swap between volume and one slice
			case GLFW_KEY_V:
				_drawVolume = !_drawVolume;
				break;

			// switch between the different types of CLAHE
			case GLFW_KEY_D:
				_textureMode = TextureMode::_RAW;
				_currTexture = _dicomVolumeTexture;
				break;
			case GLFW_KEY_C:
				_textureMode = TextureMode::_CLAHE;
				_currTexture = _claheDicomVolumeTexture;
				break;
			case GLFW_KEY_F:
				_textureMode = TextureMode::_FOCUSED;
				_currTexture = _focusedDicomVolumeTexture;
				break;
			case GLFW_KEY_1:
				_currTexture = _claheDicomVolumeTexture;
				break;
			case GLFW_KEY_2:
				_currTexture = _claheFullMasked;
				break;
			case GLFW_KEY_3:
				_currTexture = _claheOnlyMask;
				break;
			case GLFW_KEY_M:
				_useMask = !_useMask;
				break;

			// Change Interaction Mode
			case GLFW_KEY_B:
				if (_interactionMode == InteractionMode::_MOVE) {
					_interactionMode = InteractionMode::_CHANGE;
					printf("Interaction Mode: CHANGE BLOCK DIMENSIONS\n");
				}
				else {
					_interactionMode = InteractionMode::_MOVE;
					printf("Interaction Mode: MOVE BLOCK\n");
				}
				break;

			// Increase/Decrease the CLip Limit 
			case GLFW_KEY_EQUAL:
				if (mods == GLFW_MOD_SHIFT) {
					clipLimit3D += 0.1f;
				}				
				newTexture = comp.ComputeFocused3D_CLAHE(min3D, max3D, clipLimit3D);
				if (newTexture != 0)
					_focusedDicomVolumeTexture = newTexture;
				break;
			case GLFW_KEY_MINUS:
				clipLimit3D -= 0.1f;
				newTexture = comp.ComputeFocused3D_CLAHE(min3D, max3D, clipLimit3D);
				if (newTexture != 0)
					_focusedDicomVolumeTexture = newTexture;
				break;

			// Increase/Decrease the number of SB
			case GLFW_KEY_P:
				bool result;
				if (mods == GLFW_MOD_SHIFT) {
					result = comp.ChangePixelsPerSB(false);
				}
				else {
					result = comp.ChangePixelsPerSB(true);
				}
				if (result) {
					_focusedDicomVolumeTexture = comp.ComputeFocused3D_CLAHE(min3D, max3D, clipLimit3D);
				}
				break;

			// Move/Change the Focused Region 
			case GLFW_KEY_X:
				if (mods == GLFW_MOD_SHIFT) {
					if (_interactionMode == InteractionMode::_MOVE) {
						min3D += glm::uvec3(step.x, 0, 0);
						max3D += glm::uvec3(step.x, 0, 0);
					}
					else if(_interactionMode == InteractionMode::_CHANGE) {
						min3D -= glm::uvec3(step.x, 0, 0);
						max3D += glm::uvec3(step.x, 0, 0);
					}
				}
				else {
					if (_interactionMode == InteractionMode::_MOVE) {
						min3D -= glm::uvec3(step.x, 0, 0);
						max3D -= glm::uvec3(step.x, 0, 0);
					}
					else if (_interactionMode == InteractionMode::_CHANGE) {
						min3D += glm::uvec3(step.x, 0, 0);
						max3D -= glm::uvec3(step.x, 0, 0);
					}
				}
				newTexture = comp.ComputeFocused3D_CLAHE(min3D, max3D, clipLimit3D);
				if (newTexture != 0)
					_focusedDicomVolumeTexture = newTexture;
				break;
			case GLFW_KEY_Y:
				if (mods == GLFW_MOD_SHIFT) {
					if (_interactionMode == InteractionMode::_MOVE) {
						min3D += glm::uvec3(0, step.y, 0);
						max3D += glm::uvec3(0, step.y, 0);
					}
					else if (_interactionMode == InteractionMode::_CHANGE) {
						min3D -= glm::uvec3(0, step.y, 0);
						max3D += glm::uvec3(0, step.y, 0);
					}
				}
				else {
					if (_interactionMode == InteractionMode::_MOVE) {
						min3D -= glm::uvec3(0, step.y, 0);
						max3D -= glm::uvec3(0, step.y, 0);
					}
					else if (_interactionMode == InteractionMode::_CHANGE) {
						min3D += glm::uvec3(0, step.y, 0);
						max3D -= glm::uvec3(0, step.y, 0);
					}
				}
				newTexture = comp.ComputeFocused3D_CLAHE(min3D, max3D, clipLimit3D);
				if (newTexture != 0)
					_focusedDicomVolumeTexture = newTexture;
				break;
			case GLFW_KEY_Z:
				if (mods == GLFW_MOD_SHIFT) {
					if (_interactionMode == InteractionMode::_MOVE) {
						min3D += glm::uvec3(0, 0, step.z);
						max3D += glm::uvec3(0, 0, step.z);
					}
					else if (_interactionMode == InteractionMode::_CHANGE) {
						min3D -= glm::uvec3(0, 0, step.z);
						max3D += glm::uvec3(0, 0, step.z);
					}
				}
				else {
					if (_interactionMode == InteractionMode::_MOVE) {
						min3D -= glm::uvec3(0, 0, step.z);
						max3D -= glm::uvec3(0, 0, step.z);
					}
					else if (_interactionMode == InteractionMode::_CHANGE) {
						min3D += glm::uvec3(0, 0, step.z);
						max3D -= glm::uvec3(0, 0, step.z);
					}
				}
				newTexture = comp.ComputeFocused3D_CLAHE(min3D, max3D, clipLimit3D);
				if (newTexture != 0)
					_focusedDicomVolumeTexture = newTexture;
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

	printf("(%.0f, %.0f, %.0f)\n", toPrint.x, toPrint.y, toPrint.z);
}

////////////////////////////////////////////////////////////////////////////////