#include <GL/glew.h>
#include <GLFW/glfw3.h>


#include <iostream>
#include <stdio.h>

#include "SceneManager.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////
// Helper Functions

bool printVersions() {
    if (const GLubyte* renderer = glGetString(GL_RENDERER))
        printf("OpenGL Renderer: %s\n", renderer);
    else {
        printf("Failed to get OpenGL renderer\n");
        return false;
    }

    if (const GLubyte* version = glGetString(GL_VERSION))
        printf("OpenGL Version: %s\n", version);
    else {
        printf("Failed to get OpenGL version\n");
        return false;
    }
    return true;
}

void error_callback(int error, const char* description) {
    fprintf(stderr, "Error: %s\n", description);
    throw;
}

void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, 
    GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
    fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n", 
        (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity, message);
    throw;
}

////////////////////////////////////////////////////////////////////////////////
// Main

int main(int argc, char** argv){
    glfwSetErrorCallback(error_callback);

    // Initialize GLFW
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    // Make the Window for the Scene 
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    const char* title = "CLAHE";
    int width = 800;
    int height = 800;
    if (!SceneManager::CreateWindow(title, width, height)) {
        fprintf(stderr, "Failed to create window\n");
        return 1;
    }

    // Debug Messages
    if (!printVersions()) {
        return 1;
    }

    // Initialize GLEW
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        printf("Failed to initialize GLEW!\n");
        return 1;
    }

    // During init, enable debug output
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);

    // Main Render Loop
    SceneManager::InitScene();
    while (SceneManager::WindowOpen()) {

        SceneManager::Update();
        SceneManager::Draw();
    }

    SceneManager::ClearScene();
    glfwTerminate();

    return 0;
}

////////////////////////////////////////////////////////////////////////////////