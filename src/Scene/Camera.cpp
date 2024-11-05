#include "Camera.h"

Camera::Camera() {
	setFOV(0.25f * XMVectorGetX(g_XMPi), 1.0f, 1.0f, 1000.0f);

	XMMATRIX P = XMMatrixPerspectiveFovLH(FOVY, aspect, nearPlane, farPlane);
	XMStoreFloat4x4(&projMat, P);
}

void Camera::setFOV(float FOVY, float aspect, float nearPlane, float farPlane) {
	FOVY = FOVY;
	aspect = aspect;
	nearPlane = nearPlane;
	farPlane = farPlane;
}

void Camera::rotateY(float angle) {
	XMMATRIX R = XMMatrixRotationY(angle);

	XMStoreFloat3(&right, XMVector3TransformNormal(XMLoadFloat3(&right), R));
	XMStoreFloat3(&up, XMVector3TransformNormal(XMLoadFloat3(&up), R));
	XMStoreFloat3(&forward, XMVector3TransformNormal(XMLoadFloat3(&forward), R));
}

void Camera::rotateX(float angle) {
	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&right), angle);

	XMStoreFloat3(&up, XMVector3TransformNormal(XMLoadFloat3(&up), R));
	XMStoreFloat3(&forward, XMVector3TransformNormal(XMLoadFloat3(&forward), R));
}

void Camera::updateViewMat() {
	XMVECTOR R = XMLoadFloat3(&right);
	XMVECTOR U = XMLoadFloat3(&up);
	XMVECTOR F = XMLoadFloat3(&forward);
	XMVECTOR P = XMLoadFloat3(&position);

	F = XMVector3Normalize(F);
	U = XMVector3Normalize(XMVector3Cross(F, R));
	R = XMVector3Cross(U, F);

	float x = -XMVectorGetX(XMVector3Dot(P, R));
	float y = -XMVectorGetX(XMVector3Dot(P, U));
	float z = -XMVectorGetX(XMVector3Dot(P, F));

	XMStoreFloat3(&right, R);
	XMStoreFloat3(&up, U);
	XMStoreFloat3(&forward, F);

	viewMat(0, 0) = right.x;
	viewMat(1, 0) = right.y;
	viewMat(2, 0) = right.z;
	viewMat(3, 0) = x;

	viewMat(0, 1) = up.x;
	viewMat(1, 1) = up.y;
	viewMat(2, 1) = up.z;
	viewMat(3, 1) = y;

	viewMat(0, 2) = forward.x;
	viewMat(1, 2) = forward.y;
	viewMat(2, 2) = forward.z;
	viewMat(3, 2) = z;

	viewMat(0, 3) = 0.0f;
	viewMat(1, 3) = 0.0f;
	viewMat(2, 3) = 0.0f;
	viewMat(3, 3) = 1.0f;
}

XMFLOAT4X4 Camera::getViewMat() {
	return viewMat;
}

XMFLOAT4X4 Camera::getProjMat() {
	return projMat;
}
