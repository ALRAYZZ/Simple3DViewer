// Header for camera class - manages camera position, orientation and MVP matrix

#pragma once
#include <DirectXMath.h>
#include <QVector3D>

class Camera
{
public:
	Camera();
	void orbit(float dx, float dy); // Adjust camera angles based on mouse movement
	void zoom(float delta); // Adjust camera distance based on scroll input
	DirectX::XMFLOAT4X4 getMVPMatrix(float aspectRatio) const; // Get the combined Model-View-Projection matrix

private:
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 target;
	float distance;
	float yaw;
	float pitch;

	void updatePosition(); // Recalculate camera position based on spherical coordinates
};