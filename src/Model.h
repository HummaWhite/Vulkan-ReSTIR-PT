#pragma once

#include <iostream>
#include <variant>
#include <optional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <pugixml.hpp>

#include "Alignment.h"
#include "util/File.h"
#include "Material.h"

struct MeshVertex {
	std140(glm::vec3, pos);
	std140(float, uvx);
	std140(glm::vec3, norm);
	std140(float, uvy);
};

struct MeshInstance {
	uint32_t offset = 0;
	uint32_t numIndices = 0;
	int materialIdx = 0;
};

class ModelInstance {
public:
	friend class Resource;
	friend class Scene;

	ModelInstance() = default;
	~ModelInstance() {}

	void setPos(glm::vec3 pos) { mPos = pos; }
	void setPos(float x, float y, float z) { mPos = { x, y, z }; }
	void move(glm::vec3 vec) { mPos += vec; }
	void rotateLocal(float angle, glm::vec3 axis);
	void rotateWorld(float abgle, glm::vec3 axis);
	void setScale(glm::vec3 scale) { mScale = scale; }
	void setScale(float x, float y, float z) { mScale = { x, y, z }; }
	void setRotation(glm::vec3 angle) { mRotation = angle; }
	void setRotation(float yaw, float pitch, float roll) { mRotation = { yaw, pitch, roll }; }
	void setSize(float size) { mScale = glm::vec3(size); }
	void setName(const std::string& name) { mName = name; }
	void setPath(const File::path& path) { mPath = path; }

	glm::vec3 pos() const { return mPos; }
	glm::vec3 scale() const { return mScale; }
	glm::vec3 rotation() const { return mRotation; }
	glm::mat4 modelMatrix() const;
	std::string name() const { return mName; }
	File::path path() const { return mPath; }

	std::vector<MeshInstance>& meshInstances() { return mMeshInstances; }

private:
	ModelInstance* copy() const;

private:
	std::vector<MeshInstance> mMeshInstances;
	glm::vec3 mPos = glm::vec3(0.0f);
	glm::vec3 mScale = glm::vec3(1.0f);
	glm::vec3 mRotation = glm::vec3(0.0f);
	glm::mat4 mRotMatrix = glm::mat4(1.0f);

	std::string mName;
	File::path mPath;
};