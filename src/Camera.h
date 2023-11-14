#pragma once

#include <iostream>
#include <random>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class Camera {
public:
	Camera(glm::vec3 pos = glm::vec3(0, 0, 0), glm::vec3 angle = glm::vec3(90.0f, 0.0f, 0.0f));

	void move(glm::vec3 vec);
	void roll(float rolAngle);
	void rotate(glm::vec3 rotAngle);
	void changeFOV(float offset);
	void setFOV(float fov);
	void lookAt(glm::vec3 focus) { setDir(focus - mPos); }
	void setDir(glm::vec3 dir);
	void setPos(glm::vec3 pos);
	void setAngle(glm::vec3 angle);
	void setFilmSize(glm::uvec2 size);
	void setPlanes(float nearZ, float farZ);
	void setLensRadius(float radius) { mLensRadius = radius; }
	void setFocalDist(float dist) { mFocalDist = dist; }

	glm::vec3 pos() const { return mPos; }
	glm::vec3 angle() const { return mAngle; }
	glm::vec3 front() const { return mFront; }
	glm::vec3 right() const { return mRight; }
	glm::vec3 up() const { return mUp; }
	glm::ivec2 filmSize() const { return mFilmSize; }
	float FOV() const { return mFOV; }
	float aspect() const { return static_cast<float>(mFilmSize.x) / mFilmSize.y; }
	float lensRadius() const { return mLensRadius; }
	float focalDist() const { return mFocalDist; }

	glm::mat4 viewMatrix() const { return mViewMatrix; }
	glm::mat4 projMatrix() const { return mProjMatrix; }

	void nextFrame(std::default_random_engine& rng);

	void update();

private:
	glm::mat4 mViewMatrix;
	glm::mat4 mProjMatrix;
	glm::mat4 mProjView;
	glm::mat4 mLastProjView;

	glm::vec3 mPos;
	float mFOV = 45.0f;

	glm::vec3 mAngle;
	float mNear = 1e-3f;

	glm::vec3 mFront;
	float mFar = 1e3f;

	glm::vec3 mRight;
	float mLensRadius = 0.0f;

	glm::vec3 mUp = glm::vec3(0.0f, 0.0f, 1.0f);
	float mFocalDist = 1.0f;

	glm::uvec2 mFilmSize;
	uint32_t mFrameIndex = 0;
	uint32_t seed;
};
