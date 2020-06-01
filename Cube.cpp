////////////////////////////////////////
// Cube.cpp
////////////////////////////////////////

#include "Cube.h"

////////////////////////////////////////////////////////////////////////////////
// Constructor/Destructor

Cube::Cube() {

	static const float cubeVertices[24]{
			-0.5f, -0.5f,  0.5f,
			0.5f, -0.5f,  0.5f,
			0.5f,  0.5f,  0.5f,
			-0.5f,  0.5f,  0.5f,
			-0.5f, -0.5f, -0.5f,
			0.5f, -0.5f, -0.5f,
			0.5f,  0.5f, -0.5f,
			-0.5f,  0.5f, -0.5f
	};
	static const GLuint cubeIndices[36]{
		0, 1, 2, 2, 3, 0,
		1, 5, 6, 6, 2, 1,
		7, 6, 5, 5, 4, 7,
		4, 0, 3, 3, 7, 4,
		4, 5, 1, 1, 0, 4,
		3, 2, 6, 6, 7, 3
	};

	glGenVertexArrays(1, &_VAO);
	glGenBuffers(1, &_VBO);
	glGenBuffers(1, &_EBO);

	glBindVertexArray(_VAO);

	glBindBuffer(GL_ARRAY_BUFFER, _VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

}

Cube::~Cube() {
	glDeleteVertexArrays(1, &_VAO);
	glDeleteBuffers(1, &_VBO);
	glDeleteBuffers(1, &_EBO);
}

////////////////////////////////////////////////////////////////////////////////

void Cube::Draw(GLuint shader, const glm::mat4& VPMatrix, const glm::vec3& CamPos, 
				GLuint texture, GLuint maskTexture, bool useMask) {

	glUseProgram(shader);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, texture);
	//glBindImageTexture(0, texture, 0, GL_TRUE, 1, GL_READ_ONLY, GL_R16UI);	
	glUniform1i(glGetUniformLocation(shader, "Volume"), 0);

	//glActiveTexture(GL_TEXTURE1);
	//glBindTexture(GL_TEXTURE_3D, maskTexture);
	//glUniform1i(glGetUniformLocation(shader, "Mask"), 1);
	glBindImageTexture(1, maskTexture, 0, GL_TRUE, 1, GL_READ_ONLY, GL_R8UI);
	glUniform1i(glGetUniformLocation(shader, "useMask"), useMask);

	glUniformMatrix4fv(glGetUniformLocation(shader, "MVP"), 1, GL_FALSE, (float*)&VPMatrix);
	glUniform3f(glGetUniformLocation(shader, "CameraPosition"), CamPos.x, CamPos.y, CamPos.z);
	//glUniform3f(glGetUniformLocation(shader, "CameraPosition"), 0, 0, 0);

	glBindVertexArray(_VAO);
	glDrawElements(GL_TRIANGLES, _numIndices, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

////////////////////////////////////////////////////////////////////////////////