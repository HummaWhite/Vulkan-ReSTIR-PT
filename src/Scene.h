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
	zvk::Buffer* vertices = nullptr;
	zvk::Buffer* indices = nullptr;
	zvk::Buffer* materials = nullptr;
	zvk::Buffer* materialIds = nullptr;
	zvk::Buffer* camera = nullptr;
	std::vector<zvk::Image*> textures;

	uint32_t numVertices = 0;
	uint32_t numIndices = 0;
	uint32_t numMaterials = 0;

	zvk::DescriptorSetLayout* cameraDescLayout = nullptr;
	zvk::DescriptorSetLayout* resourceDescLayout = nullptr;
	vk::DescriptorSet cameraDescSet;
	vk::DescriptorSet resourceDescSet;

private:
	zvk::DescriptorPool* mDescriptorPool = nullptr;
};