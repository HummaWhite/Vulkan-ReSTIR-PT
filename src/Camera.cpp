#include "Camera.h"

Camera::Camera(glm::vec3 pos, glm::vec3 angle) :
	mPos(pos), mAngle(angle) {
	update();
}

void Camera::roll(float angle) {
	mAngle.z += angle;
	update();
}

void Camera::rotate(glm::vec3 angle) {
	mAngle += angle;
	mAngle.y = glm::clamp(mAngle.y, 0.f, 90.f);
	update();
}

void Camera::changeFOV(float offset) {
	mFOV -= glm::radians(offset);
	mFOV = glm::clamp(mFOV, .1f, 90.f);
}

void Camera::setFOV(float fov) {
	mFOV = fov;
	mFOV = glm::clamp(mFOV, .1f, 90.f);
}

void Camera::setDir(glm::vec3 dir) {
	dir = glm::normalize(dir);
	mAngle.y = glm::degrees(asin(dir.z / length(dir)));
	mAngle.x = glm::degrees(asin(dir.y / length(glm::vec2(dir))));

	if (dir.x < 0) {
		mAngle.x = 180.0f - mAngle.x;
	}
	update();
}

void Camera::setAngle(glm::vec3 angle) {
	mAngle = angle;
	update();
}

glm::mat4 Camera::viewMatrix() const {
	float x = glm::sin(glm::radians(mAngle.x)) * glm::cos(glm::radians(mAngle.y));
	float y = glm::cos(glm::radians(mAngle.x)) * glm::cos(glm::radians(mAngle.y));
	float z = glm::sin(glm::radians(mAngle.y));
	glm::vec3 lookingAt = mPos + glm::vec3(x, y, z);
	glm::mat4 view = glm::lookAt(mPos, lookingAt, mUp);
	return view;
}

void Camera::update()
{
	float x = glm::sin(glm::radians(mAngle.x)) * glm::cos(glm::radians(mAngle.y));
	float y = glm::cos(glm::radians(mAngle.x)) * glm::cos(glm::radians(mAngle.y));
	float z = glm::sin(glm::radians(mAngle.y));

	mFront = glm::normalize(glm::vec3(x, y, z));
	const glm::vec3 u(0.0f, 0.0f, 1.0f);
	mRight = glm::normalize(glm::cross(mFront, u));

	auto rotMatrix = glm::rotate(glm::mat4(1.0f), mAngle.z, mFront);
	mRight = glm::normalize(glm::vec3(rotMatrix * glm::vec4(mRight, 1.0f)));
	mUp = glm::normalize(glm::cross(mRight, mFront));
}
