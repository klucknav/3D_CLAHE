////////////////////////////////////////
// Display.frag 
// when used with display.vert displays a texture to the window
////////////////////////////////////////

#version 440 core

layout (binding = 0) uniform sampler2D texture;

in VertexData {
	vec2 uv;
} inData;

out vec4 FragColor;

void main()
{
	//FragColor = vec4(1.0, 0.0, 0.0, 1.0);
	//FragColor = vec4(inData.uv, 1.0, 1.0);
	FragColor = vec4(texture(texture, inData.uv).rrr, 1);
	//FragColor = textureLod(environmentMap, inData.uv, 1.0);
}