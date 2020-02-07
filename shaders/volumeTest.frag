#version 440 core

out vec4 FragColor;

in vs_out {
	vec3 rd;
	vec3 sp;
} i;

uniform float StepSize = 4;
uniform vec3 CameraPosition;
uniform sampler3D Volume;

void main() {
	FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}