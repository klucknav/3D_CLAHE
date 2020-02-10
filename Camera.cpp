////////////////////////////////////////
// Camera.cpp
////////////////////////////////////////

#include "Camera.h"

#include <glm/gtx/euler_angles.hpp>

////////////////////////////////////////////////////////////////////////////////

Camera::Camera() {
	Reset();
}

////////////////////////////////////////////////////////////////////////////////

void Camera::Update() {

	// Compute camera world matrix
	glm::mat4 world(1);
	world[3][2] = _distance;
	world = glm::eulerAngleY(glm::radians(-_azimuth)) * glm::eulerAngleX(glm::radians(-_incline)) * world;

	// Compute the Camera position
	_cameraPosition = world * glm::vec4(0, 0, 0, 1);

	// Compute view matrix (inverse of world matrix)
	glm::mat4 view = glm::inverse(world);

	// Compute perspective projection matrix
	glm::mat4 project = glm::perspective(glm::radians(_FOV), _aspect, _nearPlane, _farPlane);

	// Compute final view-projection matrix
	_viewProjectMtx = project * view;
}

void Camera::UpdateAngles(int a, int i) {
	_azimuth = _azimuth + a * _angleRate;
	_incline = glm::clamp(_incline - i * _angleRate, -90.0f, 90.0f);
}

void Camera::UpdateDistance(int d) {
	_distance = glm::clamp(_distance * (1.0f - d * _distanceRate), 0.01f, 1000.0f);
}

////////////////////////////////////////////////////////////////////////////////

void Camera::Reset() {
	_FOV = 45.0f;
	_aspect = 1.33f;
	_nearPlane = 0.1f;
	_farPlane = 100.0f;
	
	_distance = 10.0f;
	_azimuth = 0.0f;
	_incline = 20.0f;
}

////////////////////////////////////////////////////////////////////////////////