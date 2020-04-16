////////////////////////////////////////
// volume.frag
// displays volume texture 
////////////////////////////////////////

#version 430
//#version 440 core

in vs_out {
	vec3 rayDirection;
	vec3 screenPixel;
} inVector;

out vec4 FragColor;

uniform float Threshold = 0.1;	// don't draw when alpha value is less than 10%
uniform float Exposure = 1.0;	// brighten volume 1.8
uniform float Density = 0.1;	// 
uniform float StepSize = .002;
uniform vec3 CameraPosition;

// Volume and mask Textures 
uniform sampler3D Volume;
uniform int useMask;
layout(r8ui, binding = 1) uniform uimage3D Mask;

////////////////////////////////////////////////////////////////////////////////
// Helper functions

vec2 RayCube(vec3 rayOrigin, vec3 rayDirection, vec3 extents) {
    vec3 tMin = (-extents - rayOrigin) / rayDirection;
    vec3 tMax = (extents - rayOrigin) / rayDirection;
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    return vec2(max(max(t1.x, t1.y), t1.z), min(min(t2.x, t2.y), t2.z));
}
float RayPlane(vec3 rayOrigin, vec3 rayDirection, vec3 planePoint, vec3 planeNormal) {
	float dist = dot(planeNormal, rayDirection);
	float t = dot(planePoint - rayOrigin, planeNormal);
	return dist > 1e-5 ? (t / dist) : (t > 0 ? 1e5 : -1e5);
}

vec4 Sample(vec3 samplePoint) {
	vec4 colorSample = textureLod(Volume, samplePoint, 0.0).rrrr;

	if (useMask == 1) {
//		float maskVal = textureLod(Mask, samplePoint, 0.0).x;
		ivec3 dims = imageSize(Mask);
		ivec3 samplePt = ivec3(samplePoint * dims);
//		samplePt.x = dims.x - samplePt.x;
		uint maskVal = imageLoad(Mask, samplePt).x;
		if (maskVal == 0.0) {
			colorSample.a = maskVal;
		}
	}
//	colorSample.a = max(0.0, (colorSample.a - Threshold) / (1.0 - Threshold)); // subtractive for soft edges
//	colorSample.rgb *= Exposure;
	colorSample.a *= Density;
	return colorSample;
}


////////////////////////////////////////////////////////////////////////////////
// Main
void main() {
	vec3 rayOrigin = CameraPosition;
	vec3 rayDirection = normalize(inVector.rayDirection.xyz);

	vec2 intersect = RayCube(rayOrigin, rayDirection, vec3(.5));
	intersect.x = max(0, intersect.x);
	
	rayOrigin += .5; // cube has a radius of .5, transform to UVW space

	vec4 sum = vec4(0);

	uint steps = 0;
	float prevDensity = 0;
	for (float t = intersect.x; t < intersect.y;) {
		if (sum.a > .98 || steps > 750) break;

		vec3 point = rayOrigin + rayDirection * t;
		vec4 color = Sample(point);

		if (color.a > .01){
			if (prevDensity < .01) {
				// first time entering volume, binary subdivide to get closer to entrance point
				float t0 = t - StepSize * 4;
				float t1 = t;
				float tm;
				#define BINARY_SUBDIV tm = (t0 + t1) * .5; point = rayOrigin + rayDirection * tm; if (Sample(point).a > .01) t1 = tm; else t0 = tm;
				BINARY_SUBDIV
				BINARY_SUBDIV
				BINARY_SUBDIV
				BINARY_SUBDIV
				#undef BINARY_SUBDIV
				t = tm;
				color = Sample(point);
			}
			
			color.rgb *= color.a;
			sum += color * (1 - sum.a);
		}

		steps++; // only count steps through the volume

		prevDensity = color.a;
		t += color.a > .01 ? StepSize : StepSize * 4; // step farther if not in dense part
	}

	sum.a = clamp(sum.a, 0.0, 1.0);
	FragColor = sum;		
}
////////////////////////////////////////////////////////////////////////////////