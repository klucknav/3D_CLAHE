////////////////////////////////////////
// SceneManager.h
////////////////////////////////////////

#include "SceneManager.h"
#include "Shader.h"
#include "ImageLoader.h"
#include "ComputeCLAHE.h"

#include <stdio.h>
#include <chrono>

// Window Variables
int SceneManager::_windowWidth;
int SceneManager::_windowHeight;
GLFWwindow* SceneManager::_window;

// Scene Variables
Camera* SceneManager::_camera;
// 3D Volume
ImageLoader* _dicomVolume;
Cube* SceneManager::_dicomCube;
GLuint SceneManager::_dicomVolumeTexture;
GLuint SceneManager::_dicomMaskTexture;
// CLAHE Textures
GLuint SceneManager::_3D_CLAHE;
GLuint SceneManager::_FocusedCLAHE;
GLuint SceneManager::_MaskedCLAHE;
// CLAHE Shader
GLuint SceneManager::_volumeShader;

// CLAHE Variables
ComputeCLAHE comp;
glm::uvec3 numSB_3D = glm::uvec3(4, 4, 2);
glm::uvec3 min3D = glm::uvec3(200, 200, 40);
glm::uvec3 max3D = glm::uvec3(400, 400, 90);
float clipLimit3D = 0.85f;

GLuint _currTexture;
bool _useMask = false;
enum class TextureMode {
	_RAW,
	_CLAHE,
	_FOCUSED,
	_MASKED
};
TextureMode _textureMode;
enum class InteractionMode {
	_MOVE, 
	_CHANGE
};
InteractionMode _interactionMode;


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

	glEnable(GL_MULTISAMPLE);

	return 1;
}

bool SceneManager::WindowOpen() {
	return !glfwWindowShouldClose(_window);
}

////////////////////////////////////////////////////////////////////////////////
// Constructor/Destructor for the Scene

void SceneManager::InitScene() {

	////////////////////////////////////////////////////////////////////////////
	_camera = new Camera();

	// load the shaders
	_volumeShader = LoadShaders("volume.vert", "volume.frag");
	
	////////////////////////////////////////////////////////////////////////////
	// 3D CLAHE - Cube Volume 
	std::cerr << "\n\n----- Load 3D DICOM -----\n";
	_dicomCube = new Cube();
	std::string folderPath = std::string("C:/Users/kroth/Documents/UCSD/Grad/Thesis/clahe_2/Larry_2017");
	std::string maskPath = std::string("C:/Users/kroth/Documents/UCSD/Grad/Thesis/clahe_2/Larry_2017/mask");
	ImageLoader* _dicomVolume = new ImageLoader(folderPath, maskPath, false);
	glm::vec3 volDim = _dicomVolume->GetImageDimensions();
	_dicomVolumeTexture = _dicomVolume->GetTextureID();
	_dicomMaskTexture = _dicomVolume->GetMaskID();

	////////////////////////////////////////////////////////////////////////////
	// 3D CLAHE with Compute Shaders
	unsigned int outputGrayvals_3D = 65536;
	unsigned int inputGrayvals_3D = 65536;
	unsigned int numOrgans = 4;

	comp.Init(_dicomVolumeTexture, _dicomMaskTexture, volDim, outputGrayvals_3D, inputGrayvals_3D, numOrgans);

	_3D_CLAHE = comp.Compute3D_CLAHE(numSB_3D, clipLimit3D);
	_FocusedCLAHE = comp.ComputeFocused3D_CLAHE(min3D, max3D, clipLimit3D);	
	_MaskedCLAHE = comp.ComputeMasked3D_CLAHE(clipLimit3D);

	////////////////////////////////////////////////////////////////////////////

	_textureMode = TextureMode::_RAW;
	_interactionMode = InteractionMode::_MOVE;
	_currTexture = _dicomVolumeTexture;	// raw dicom
}

void SceneManager::ClearScene() {

	glDeleteProgram(_volumeShader);
	
	delete _camera;
	delete _dicomCube;
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
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	_dicomCube->Draw(_volumeShader, _camera->GetViewProjectMtx(), _camera->GetCamPos(), _currTexture, _dicomMaskTexture, _useMask);

	// Swap buffers
	glfwSwapBuffers(_window);
}

////////////////////////////////////////////////////////////////////////////////
// Callbacks 

void SceneManager::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {

	glm::uvec3 step = glm::uvec3(20, 20, 10);
	float clipStep = 0.05f;
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

			// switch between the different types of CLAHE
			case GLFW_KEY_D:
				_textureMode = TextureMode::_RAW;
				_currTexture = _dicomVolumeTexture;
				break;
			case GLFW_KEY_C: // 3D CLAHE
				_textureMode = TextureMode::_CLAHE;
				_currTexture = _3D_CLAHE;
				break;
			case GLFW_KEY_F: // Focused CLAHE
				_textureMode = TextureMode::_FOCUSED;
				_currTexture = _FocusedCLAHE;
				break;
			case GLFW_KEY_M: // Masked CLAHE 
				_textureMode = TextureMode::_MASKED;
				_currTexture = _MaskedCLAHE;
				break;
			case GLFW_KEY_O: // show just the organs
				_useMask = !_useMask;
				break;

			// Change Interaction Mode for Focused CLAHE
			case GLFW_KEY_B:
				if (_textureMode == TextureMode::_FOCUSED) {
					if (_interactionMode == InteractionMode::_MOVE) {
						_interactionMode = InteractionMode::_CHANGE;
						printf("Interaction Mode: CHANGE BLOCK DIMENSIONS\n");
					}
					else {
						_interactionMode = InteractionMode::_MOVE;
						printf("Interaction Mode: MOVE BLOCK\n");
					}
				}
				break;

			// Increase/Decrease the CLip Limit 
			case GLFW_KEY_EQUAL:
				if (mods == GLFW_MOD_SHIFT) { // '+' Key 
					clipLimit3D += clipStep;
				}
				updateVolume();
				break;
			case GLFW_KEY_MINUS:
				clipLimit3D -= clipStep;
				updateVolume();
				break;

			// Increase/Decrease the number of SB for CLAHE/Focused CLAHE
			case GLFW_KEY_S:
				// inc/dec the number of SB for CLAHE
				if (_textureMode == TextureMode::_CLAHE) {
					if (mods == GLFW_MOD_SHIFT) { // inc numSB
						numSB_3D += glm::uvec3(1, 1, 1);
						printf("numSB: (%d, %d, %d)\n", numSB_3D.x, numSB_3D.y, numSB_3D.z);
					}
					else {
						numSB_3D -= glm::uvec3(1, 1, 1);
						if (numSB_3D.x < 1) numSB_3D.x = 1;
						if (numSB_3D.y < 1) numSB_3D.y = 1;
						if (numSB_3D.z < 1) numSB_3D.z = 1;
						printf("numSB: (%d, %d, %d)\n", numSB_3D.x, numSB_3D.y, numSB_3D.z);
					}
					_3D_CLAHE = comp.Compute3D_CLAHE(numSB_3D, clipLimit3D);
					_currTexture = _3D_CLAHE;
				}
				// inc/dec the number of pixels per SB for Focused CLAHE
				else if (_textureMode == TextureMode::_FOCUSED) {
					bool changeOK; 
					if (mods == GLFW_MOD_SHIFT) { // inc numSB
						changeOK = comp.ChangePixelsPerSB(false);
					}
					else {
						changeOK = comp.ChangePixelsPerSB(true);
					}
					if (changeOK) {
						_FocusedCLAHE = comp.ComputeFocused3D_CLAHE(min3D, max3D, clipLimit3D);
						_currTexture = _FocusedCLAHE;
					}
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
				if (newTexture) {
					_FocusedCLAHE = newTexture;
					_currTexture = newTexture;
				}
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
				if (newTexture) {
					_FocusedCLAHE = newTexture;
					_currTexture = newTexture;
				}
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
				if (newTexture) {
					_FocusedCLAHE = newTexture;
					_currTexture = newTexture;
				}
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

void SceneManager::updateVolume() {
	if (_textureMode == TextureMode::_CLAHE) {
		_3D_CLAHE = comp.Compute3D_CLAHE(numSB_3D, clipLimit3D);
		_currTexture = _3D_CLAHE;
	}
	else if (_textureMode == TextureMode::_FOCUSED) {
		_FocusedCLAHE = comp.ComputeFocused3D_CLAHE(min3D, max3D, clipLimit3D);
		_currTexture = _FocusedCLAHE;
	}
	else if (_textureMode == TextureMode::_MASKED) {
		_MaskedCLAHE = comp.ComputeMasked3D_CLAHE(clipLimit3D);
		_currTexture = _MaskedCLAHE;
	}
}

////////////////////////////////////////////////////////////////////////////////