#pragma once

#include <iostream>
#include <variant>
#include <optional>
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <pugixml.hpp>

#include "core/Alignment.h"
#include "util/File.h"
#include "Material.h"

struct MeshVertex {
	constexpr static vk::VertexInputBindingDescription bindingDescription() {
		return vk::VertexInputBindingDescription(0, sizeof(MeshVertex), vk::VertexInputRate::eVertex);
	}

	constexpr static std::array<vk::VertexInputAttributeDescription, 4> attributeDescription() {
		return {
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(MeshVertex, pos)),
			vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32Sfloat, offsetof(MeshVertex, uvx)),
			vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32B32Sfloat, offsetof(MeshVertex, norm)),
			vk::VertexInputAttributeDescription(3, 0, vk::Format::eR32Sfloat, offsetof(MeshVertex, uvy))
		};
	}

	std140(glm::vec3, pos);
	std140(float, uvx);
	std140(glm::vec3, norm);
	std140(float, uvy);
};

struct MeshInstance {
	uint32_t offset = 0;
	uint32_t numIndices = 0;
	int materialIdx = InvalidResourceIdx;
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

	uint32_t offset() const { return mOffset; }
	uint32_t numMeshes() const { return mNumMeshes; }
	glm::vec3 pos() const { return mPos; }
	glm::vec3 scale() const { return mScale; }
	glm::vec3 rotation() const { return mRotation; }
	glm::mat4 modelMatrix() const;
	std::string name() const { return mName; }
	File::path path() const { return mPath; }

private:
	ModelInstance* copy() const;

private:
	uint32_t mOffset = 0;
	uint32_t mNumMeshes = 0;

	glm::vec3 mPos = glm::vec3(0.0f);
	glm::vec3 mScale = glm::vec3(1.0f);
	glm::vec3 mRotation = glm::vec3(0.0f);
	glm::mat4 mRotMatrix = glm::mat4(1.0f);

	std::string mName;
	File::path mPath;
};