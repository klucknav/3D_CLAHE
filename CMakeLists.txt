project(clahe)
cmake_minimum_required(VERSION 3.15)

set(DCMTK_HOME "C:/Users/kroth/Desktop/dcmtk-master")
set(GLFW_HOME "C:/Users/kroth/Documents/UCSD/Grad/Thesis/clahe_2/glfw")
set(GLEW_HOME "C:/Users/kroth/Documents/UCSD/Grad/Thesis/clahe_2/glew-2.1.0")
set(GLM_INCLUDE "C:/Users/kroth/Documents/UCSD/Grad/Thesis/clahe_2")
set(PROJ_DIR "C:/Users/kroth/Documents/UCSD/Grad/Thesis/clahe_2")

set(CMAKE_CXX_STANDARD 11)

add_compile_definitions(clahe SHADER_DIR="C:/Users/kroth/Documents/UCSD/Grad/Thesis/clahe_2/shaders/")

add_executable(clahe "core.h" "main.cpp" "SceneManager.cpp" "Shader.cpp"
	"ImageLoader.cpp" "Cube.cpp" "Camera.cpp" "CLAHE.cpp" "CLAHEshader.cpp")

target_include_directories(clahe PUBLIC 
	"${GLFW_HOME}/include" 
	"${GLEW_HOME}/include" 
	"${DCMTK_HOME}/include"
	"${GLM_INCLUDE}")

if(WIN32)
	target_compile_definitions(clahe PUBLIC -DGLEW_STATIC)

	target_link_directories(clahe PUBLIC "${DCMTK_HOME}/lib")
	target_link_libraries(clahe 
		"${GLFW_HOME}/lib/glfw3.lib"
		"${GLEW_HOME}/lib/Release/x64/glew32s.lib"
		"OpenGL32.lib"
		"ofstd.lib" "oflog.lib" "dcmdata.lib" "dcmimgle.lib"
		"ws2_32.lib" "wsock32.lib" "shlwapi.lib" "iphlpapi.lib" "netapi32.lib" "propsys.lib")

		

else()
	target_link_libraries(clahe 
		"libglfw.so" 
		"libGLEW.so" 
		"libGL.so")
endif()