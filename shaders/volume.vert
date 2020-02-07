////////////////////////////////////////
// volume.vert
// displays volume texture 
////////////////////////////////////////

#version 440 core

layout (location = 0) in vec3 vertex;

out vs_out {
	vec3 rayDirection;
	vec3 screenPixel;
} outVector;


uniform mat4 MVP;
uniform vec3 CameraPosition;


////////////////////////////////////////////////////////////////////////////////

void main() {
	vec4 screenPixel = MVP * vec4(vertex, 1.0);

	outVector.rayDirection = vertex - CameraPosition;
	outVector.screenPixel = screenPixel.xyw;

	gl_Position = screenPixel;
}
////////////////////////////////////////////////////////////////////////////////