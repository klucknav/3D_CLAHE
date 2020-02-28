////////////////////////////////////////
// Display.frag 
// when used with display.vert displays a texture to the window
////////////////////////////////////////

#version 440 core

layout (binding = 0) uniform sampler2D displeyTexture;

in VertexData {
	vec2 uv;
} inData;

out vec4 FragColor;

void main() {
	FragColor = vec4(texture(displeyTexture, inData.uv).rrr, 1);
}