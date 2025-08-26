// Computes position and MVP matrix for orbiting around target

#include "Camera.h"
#include <DirectXMath.h>
#include <algorithm>

using namespace DirectX;

Camera::Camera() : distance(5.0f), yaw(0.0f), pitch(30.0f)
{
	target = { 0.0f, 0.0f, 0.0f }; // Look at origin
	updatePosition();
}

void Camera::updatePosition()
{
	float yawRad = XMConvertToRadians(yaw);
	float pitchRad = XMConvertToRadians(pitch);
	position.x = target.x + distance * cos(pitchRad) * cos(yawRad);
	position.y = target.y + distance * sin(pitchRad);
	position.z = target.z + distance * cos(pitchRad) * sin(yawRad);
}

void Camera::orbit(float dx, float dy)
{
	yaw -= dx * 0.5f; // Sensitivity factor
	pitch = std::clamp(pitch + dy * 0.5f, -89.0f, 89.0f); // Prevent flipping
	updatePosition();
}

void Camera::zoom(float delta)
{
	distance = std::max(1.0f, distance - delta * 0.1f); // Prevent getting too close
	updatePosition();
}

DirectX::XMFLOAT4X4 Camera::getMVPMatrix(float aspectRatio) const
{
	XMMATRIX view = XMMatrixLookAtLH(XMLoadFloat3(&position), XMLoadFloat3(&target), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), aspectRatio, 0.1f, 100.0f);
	XMMATRIX mvp = XMMatrixIdentity() * view * proj; // Model is identity for now
	XMFLOAT4X4 result;
	XMStoreFloat4x4(&result, mvp);
	return result;
}