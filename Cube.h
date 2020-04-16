////////////////////////////////////////
// Cube.h
////////////////////////////////////////

#pragma once

#include "core.h"

class Cube {
private:
	GLuint _VAO, _VBO, _EBO;
	unsigned int _numIndices = 36;

public:
	Cube();
	~Cube();

	void Draw(GLuint shader, const glm::mat4& VPMatrix, const glm::vec3& CamPos, 
			  GLuint texture, GLuint maskTexture, bool useMask=false);
	//void update();
};