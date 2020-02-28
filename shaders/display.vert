////////////////////////////////////////
// display.vert 
// creates positions that covers the screen
////////////////////////////////////////

#version 440 core

out VertexData {
	vec2 uv;
} outData;

void main(void) {

	float x = -1.0 + float(((gl_VertexID % 3) & 1) << 2);	// ranges from -1 to 3
	float y = -1.0 + float(((gl_VertexID % 3) & 2) << 1);
	outData.uv.x = (x + 1.0) * 0.5;	// ranges from 0 to 2
	outData.uv.y = (y + 1.0) * 0.5;
	
	gl_Position = vec4(x, y, 0, 1);
}