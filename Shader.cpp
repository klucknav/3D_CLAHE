////////////////////////////////////////
// Shader.cpp
////////////////////////////////////////

#include "shader.h"


GLuint LoadSingleShader(const char* shaderFilePath, ShaderType type)
{
	// Create a shader id.
	GLuint shaderID = 0;
	if (type == ShaderType::VERTEX)
		shaderID = glCreateShader(GL_VERTEX_SHADER);
	else if (type == ShaderType::FRAGMENT)
		shaderID = glCreateShader(GL_FRAGMENT_SHADER);
	else if (type == ShaderType::COMPUTE)
		shaderID = glCreateShader(GL_COMPUTE_SHADER);

	// Try to read shader codes from the shader file.
	std::string shaderCode;

	std::string fullPath = std::string(SHADER_DIR);
	fullPath += std::string(shaderFilePath);	
	std::ifstream shaderStream(fullPath.c_str(), std::ios::in);
	if (shaderStream.is_open())
	{
		std::string Line = "";
		while (getline(shaderStream, Line))
			shaderCode += "\n" + Line;
		shaderStream.close();
	}
	else
	{
		std::cerr << "Impossible to open " << shaderFilePath << ". "
			<< "Check to make sure the file exists and you passed in the "
			<< "right filepath!"
			<< std::endl;
		return 0;
	}

	GLint Result = GL_FALSE;
	int InfoLogLength;

	// Compile Shader.
	std::cerr << "Compiling shader: " << shaderFilePath << " ... \t";
	char const* sourcePointer = shaderCode.c_str();
	glShaderSource(shaderID, 1, &sourcePointer, NULL);
	glCompileShader(shaderID);

	// Check Shader.
	glGetShaderiv(shaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if (InfoLogLength > 0)
	{
		std::vector<char> shaderErrorMessage(InfoLogLength + 1);
		glGetShaderInfoLog(shaderID, InfoLogLength, NULL, shaderErrorMessage.data());
		std::string msg(shaderErrorMessage.begin(), shaderErrorMessage.end());
		std::cerr << msg << std::endl;
		return 0;
	}
	else
	{
		if (type == ShaderType::VERTEX)
			printf("Successfully compiled vertex shader!\n");
		else if (type == ShaderType::FRAGMENT)
			printf("Successfully compiled fragment shader!\n");
		else if (type == ShaderType::COMPUTE)
			printf("Successfully compiled compute shader!\n");
	}

	return shaderID;
}

GLuint LinkProgram(GLuint shaderID_1, GLuint shaderID_2=0) {
	GLint Result = GL_FALSE;
	int InfoLogLength;

	printf("Linking program ... ");
	GLuint programID = glCreateProgram();

	glAttachShader(programID, shaderID_1);
	if (shaderID_2 != 0) {
		glAttachShader(programID, shaderID_2);
	}

	glLinkProgram(programID);

	// Check the program.
	glGetProgramiv(programID, GL_LINK_STATUS, &Result);
	glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if (InfoLogLength > 0)
	{
		std::vector<char> ProgramErrorMessage(InfoLogLength + 1);
		glGetProgramInfoLog(programID, InfoLogLength, NULL, ProgramErrorMessage.data());
		std::string msg(ProgramErrorMessage.begin(), ProgramErrorMessage.end());
		std::cerr << msg << std::endl;
		glDeleteProgram(programID);
		return 0;
	}
	else
	{
		printf("Successfully linked program!\n");
	}

	return programID;
}

GLuint LoadComputeShader(const char* computeShaderPath) {

	GLuint computeShaderID = LoadSingleShader(computeShaderPath, ShaderType::COMPUTE);

	if (computeShaderID == 0){
		return 0;
	}

	GLuint programID = LinkProgram(computeShaderID);

	// Detach and delete the shaders as they are no longer needed.
	glDetachShader(programID, computeShaderID);
	glDeleteShader(computeShaderID);

	return programID;

}

GLuint LoadShaders(const char* vertexShaderPath, const char * fragmentShaderPath) 
{
	// Create the vertex shader and fragment shader.
	GLuint vertexShaderID = LoadSingleShader(vertexShaderPath, ShaderType::VERTEX);
	GLuint fragmentShaderID = LoadSingleShader(fragmentShaderPath, ShaderType::FRAGMENT);

	// Check both shaders.
	if (vertexShaderID == 0 || fragmentShaderID == 0) return 0;

	// Link the program.
	GLuint programID = LinkProgram(vertexShaderID, fragmentShaderID);
	
	// Detach and delete the shaders as they are no longer needed.
	glDetachShader(programID, vertexShaderID);
	glDetachShader(programID, fragmentShaderID);
	glDeleteShader(vertexShaderID);
	glDeleteShader(fragmentShaderID);

	return programID;
}
