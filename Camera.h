////////////////////////////////////////
// Camera.h
////////////////////////////////////////

#pragma once

#include "core.h"

class Camera {
private:
	// Note: angles are in degrees, distance is in meters
	// Perspective controls
	float _FOV, _aspect;		// Note: FOV in degrees
	float _nearPlane, _farPlane;

	// Polar controls
	float _distance;	// Distance of camera eye position to the origin
	float _azimuth;		// Rotation of camera eye position around the Y axis
	float _incline;		// Angle of the camera eye position over the XZ plane

	// Movement controls
	const float _angleRate = 1.0f;
	const float _distanceRate = 0.005f;

	// Computed data
	glm::mat4 _viewProjectMtx;
	glm::vec3 _cameraPosition;

public:
	Camera();

	void Update();
	void Reset();

	// Update values
	void UpdateAngles(float a, float i);
	void UpdateDistance(float d);

	// Setters and Getters 
	void SetAspect(float a)		{ _aspect = a; }
	void SetDistance(float d)	{ _distance = d; }
	void SetAzimuth(float a)	{ _azimuth = a; }
	void SetIncline(float i)	{ _incline = i; }

	float GetDistance() { return _distance; }
	float GetAzimuth()	{ return _azimuth; }
	float GetIncline()	{ return _incline; }

	const glm::mat4& GetViewProjectMtx(){ return _viewProjectMtx; }
	const glm::vec3& GetCamPos()		{ return _cameraPosition; }
};

////////////////////////////////////////////////////////////////////////////////