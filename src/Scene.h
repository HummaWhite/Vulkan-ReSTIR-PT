#pragma once

#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/HostImage.h"
#include "core/Memory.h"
#include "core/Descriptor.h""
#include "Model.h"
#include "Camera.h"
#include "Resource.h"

class Scene
{
public:
	void load(const File::path& path);
	//void clear();

private:
	void addObject(ModelInstance* object);
	void addLight(ModelInstance* light, const glm::vec3& power);
	//void addMaterial(const Material& material);

	std::pair<ModelInstance*, std::optional<glm::vec3>> loadModelInstance(const pugi::xml_node& modelNode);

public:
	Camera camera;
	Resource resource;
};

class DeviceScene : public zvk::BaseVkObject {
public:
	DeviceScene(const zvk::Context* ctx, const Scene& scene, zvk::QueueIdx queueIdx);
	~DeviceScene() { destroy(); }
	void destroy();

	void initDescriptor();

private:
	void createBufferAndImages(const Scene& scene, zvk::QueueIdx queueIdx);
	void createDescriptor();

public:
	std::unique_ptr<zvk::Buffer> vertices;
	std::unique_ptr<zvk::Buffer> indices;
	std::unique_ptr<zvk::Buffer> materials;
	std::unique_ptr<zvk::Buffer> materialIds;
	std::unique_ptr<zvk::Buffer> instances;
	std::unique_ptr<zvk::Buffer> camera;
	std::vector<std::unique_ptr<zvk::Image>> textures;

	uint32_t numVertices = 0;
	uint32_t numIndices = 0;
	uint32_t numMaterials = 0;
	uint32_t numTriangles = 0;

	std::unique_ptr<zvk::DescriptorSetLayout> cameraDescLayout;
	std::unique_ptr<zvk::DescriptorSetLayout> resourceDescLayout;
	vk::DescriptorSet cameraDescSet;
	vk::DescriptorSet resourceDescSet;

private:
	std::unique_ptr<zvk::DescriptorPool> mDescriptorPool;
};