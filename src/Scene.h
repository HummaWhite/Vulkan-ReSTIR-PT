#pragma once

#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/HostImage.h"
#include "core/Memory.h"
#include "core/Descriptor.h""
#include "core/AccelerationStructure.h"
#include "Model.h"
#include "Camera.h"
#include "Resource.h"

struct LightInstance {
	std140(glm::vec3, radiance);
	std140(uint32_t, objectIdx);
};

class Scene
{
public:
	void load(const File::path& path);
	//void clear();

private:
	void loadXML(pugi::xml_node sceneNode);
	void loadIntegrator(pugi::xml_node integratorNode);
	void loadSampler(pugi::xml_node samplerNode);
	void loadCamera(pugi::xml_node cameraNode);
	void loadModels(pugi::xml_node modelNode);
	void loadEnvironmentMap(pugi::xml_node envMapNode);

	void buildLightSampler();

	std::pair<ModelInstance*, std::optional<glm::vec3>> loadModelInstance(const pugi::xml_node& modelNode);

public:
	Camera camera;
	Resource resource;
	std::vector<ObjectInstance> objectInstances;
	std::vector<LightInstance> lightInstances;
	DiscreteSampler1D<float> lightSampleTable;
	uint32_t numObjectInstances = 0;
};

class DeviceScene : public zvk::BaseVkObject {
public:
	DeviceScene(const zvk::Context* ctx, const Scene& scene, zvk::QueueIdx queueIdx);
	~DeviceScene() { destroy(); }
	void destroy();

	void initDescriptor();

private:
	void createBufferAndImages(const Scene& scene, zvk::QueueIdx queueIdx);
	void createAccelerationStructure(const Scene& scene, zvk::QueueIdx queueIdx);
	void createDescriptor();

public:
	std::unique_ptr<zvk::Buffer> vertices;
	std::unique_ptr<zvk::Buffer> indices;
	std::unique_ptr<zvk::Buffer> materials;
	std::unique_ptr<zvk::Buffer> materialIds;
	std::unique_ptr<zvk::Buffer> instances;
	std::unique_ptr<zvk::Buffer> lightInstances;
	std::unique_ptr<zvk::Buffer> lightSampleTable;
	std::vector<std::unique_ptr<zvk::Image>> textures;

	std::unique_ptr<zvk::AccelerationStructure> topAccelStructure;
	std::vector<std::unique_ptr<zvk::AccelerationStructure>> meshAccelStructures;

	uint32_t numVertices = 0;
	uint32_t numIndices = 0;
	uint32_t numMaterials = 0;
	uint32_t numTriangles = 0;

	std::unique_ptr<zvk::DescriptorSetLayout> resourceDescLayout;
	std::unique_ptr<zvk::DescriptorSetLayout> rayTracingDescLayout;
	vk::DescriptorSet resourceDescSet;
	vk::DescriptorSet rayTracingDescSet;

private:
	std::unique_ptr<zvk::DescriptorPool> mDescriptorPool;
};