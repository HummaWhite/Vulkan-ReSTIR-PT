#include "Scene.h"
#include "util/Error.h"
#include "shader/HostDevice.h"
#include "core/DebugUtils.h"

#include <sstream>
#include <pugixml.hpp>

inline float luminance(const glm::vec3& color) {
	return glm::dot(color, glm::vec3(0.299, 0.587, 0.114));
}

std::tuple<glm::vec3, glm::vec3, glm::vec3> loadTransform(const pugi::xml_node& node) {
	glm::vec3 pos, scale, rot;

	std::stringstream posStr(node.attribute("translate").as_string());
	posStr >> pos.x >> pos.y >> pos.z;

	std::stringstream scaleStr(node.attribute("scale").as_string());
	scaleStr >> scale.x >> scale.y >> scale.z;

	std::stringstream rotStr(node.attribute("rotate").as_string());
	rotStr >> rot.x >> rot.y >> rot.z;

	return { pos, scale, rot };
}

std::pair<ModelInstance*, std::optional<glm::vec3>> Scene::loadModelInstance(const pugi::xml_node& modelNode) {
	auto modelPath = modelNode.attribute("path").as_string();
	auto model = resource.openModelInstance(modelPath, glm::vec3(0.0f));

	std::string name(modelNode.attribute("name").as_string());
	model->setName(name);
	model->setPath(modelPath);

	auto transNode = modelNode.child("transform");
	auto [pos, scale, rot] = loadTransform(transNode);

	model->setPos(pos);
	model->setScale(scale.x, scale.y, scale.z);
	model->setRotation(rot);

	if (std::string(modelNode.attribute("type").as_string()) == "light") {
		glm::vec3 radiance;
		std::stringstream radStr(modelNode.child("radiance").attribute("value").as_string());
		radStr >> radiance.r >> radiance.g >> radiance.b;
		return { model, radiance };
	}

	auto matNode = modelNode.child("material");
	auto material = loadMaterial(matNode);

	if (!material) {
		return { model, std::nullopt };
	}

	for (uint32_t i = 0; i < model->numMeshes(); i++) {
		uint32_t materialIdx = resource.mMeshInstances[i + model->meshOffset()].materialIdx;
		material->textureIdx = resource.mMaterials[materialIdx].textureIdx;
		resource.mMaterials[materialIdx] = *material;
	}

	return { model, std::nullopt };
}

void Scene::load(const File::path& path) {
	//clear();
	Log::line<0>("Scene " + path.generic_string());

	pugi::xml_document doc;
	doc.load_file(path.generic_string().c_str());
	loadXML(doc.child("scene"));

	objectInstances = resource.objectInstances();
	buildLightSampler();

	Log::line<1>("Statistics");
	Log::line<2>("Vertices = " + std::to_string(resource.vertices().size()));
	Log::line<2>("Indices = " + std::to_string(resource.indices().size()));
	Log::line<2>("Mesh instances = " + std::to_string(resource.meshInstances().size()));
	Log::line<2>("Model instances = " + std::to_string(resource.modelInstances().size()));
	Log::line<2>("Materials = " + std::to_string(resource.materials().size()));
	Log::newLine();
}

void Scene::loadXML(pugi::xml_node sceneNode) {
	if (!sceneNode) {
		throw std::runtime_error("Scene: failed to load");
	}

	loadIntegrator(sceneNode.child("integrator"));
	loadSampler(sceneNode.child("sampler"));
	loadCamera(sceneNode.child("camera"));
	loadModels(sceneNode.child("modelInstances"));
}

void Scene::loadIntegrator(pugi::xml_node integratorNode) {
	std::string integStr(integratorNode.attribute("type").as_string());

	auto size = integratorNode.child("size");
	camera.setFilmSize({ size.attribute("width").as_int(), size.attribute("height").as_int() });

	auto toneMapping = integratorNode.child("toneMapping");
	// TODO: load toneMapping
	Log::line<1>("Integrator " + integStr);
}

void Scene::loadSampler(pugi::xml_node samplerNode) {
	// sampler = (std::string(samplerNode.attribute("type").as_string()) == "sobol") ? 1 : 0;
}

void Scene::loadCamera(pugi::xml_node cameraNode) {
	glm::vec3 pos, angle;
	std::string posStr(cameraNode.child("position").attribute("value").as_string());
	std::stringstream posSS(posStr);
	posSS >> pos.x >> pos.y >> pos.z;

	std::string angleStr(cameraNode.child("angle").attribute("value").as_string());
	std::stringstream angleSS(angleStr);
	angleSS >> angle.x >> angle.y >> angle.z;

	camera.setPos(pos);
	camera.setAngle(angle);
	camera.setFOV(cameraNode.child("fov").attribute("value").as_float());
	camera.setLensRadius(cameraNode.child("lensRadius").attribute("value").as_float());
	camera.setFocalDist(cameraNode.child("focalDistance").attribute("value").as_float());
	// originalCamera = previewCamera = camera;

	Log::line<1>("Camera " + std::string(cameraNode.attribute("type").as_string()));
	Log::line<2>("Position " + posStr);
	Log::line<2>("Angle " + angleStr);
	Log::line<2>("FOV " + std::to_string(camera.FOV()));
	Log::line<2>("LensRadius " + std::to_string(camera.lensRadius()));
	Log::line<2>("FocalDistance " + std::to_string(camera.focalDist()));
}

void Scene::loadModels(pugi::xml_node modelNode) {
	for (auto instance = modelNode.first_child(); instance; instance = instance.next_sibling()) {
		auto [model, radiance] = loadModelInstance(instance);

		if (radiance) {
			lightInstances.push_back({ *radiance, static_cast<uint32_t>(resource.modelInstances().size() - 1) });
		}
	}
}

void Scene::loadEnvironmentMap(pugi::xml_node envMapNode) {
	//envMap = EnvironmentMap::create(scene.child("envMap").attribute("path").as_string());
}

void Scene::buildLightSampler() {
	Log::line<1>("Light Sample Table");
	std::vector<float> powerDistrib;
	numObjectInstances = static_cast<uint32_t>(objectInstances.size());

	for (uint32_t i = 0; i < lightInstances.size(); i++) {
		auto& lightInstance = lightInstances[i];
		auto modelInstance = resource.modelInstances()[lightInstance.objectIdx];
		ObjectInstance& lightObject = objectInstances[lightInstance.objectIdx];

		float transformedSurfaceArea = resource.getModelTransformedSurfaceArea(modelInstance);
		lightObject.lightIndex = i;
		lightObject.transformedSurfaceArea = transformedSurfaceArea;

		powerDistrib.push_back(luminance(lightInstance.radiance) * transformedSurfaceArea);
	}
	lightSampleTable.build(powerDistrib);

	Log::line<2>("Num = " + std::to_string(lightSampleTable.binomDistribs[0].prob));
	Log::line<2>("Sum = " + std::to_string(lightSampleTable.binomDistribs[0].failId));
}

DeviceScene::DeviceScene(const zvk::Context* ctx, const Scene& scene, zvk::QueueIdx queueIdx) :
	BaseVkObject(ctx)
{
	createBufferAndImages(scene, queueIdx);
	createAccelerationStructure(scene, queueIdx);
	createDescriptor();
}

void DeviceScene::destroy() {
	/*
	delete cameraDescLayout;
	delete resourceDescLayout;
	delete mDescriptorPool;

	camera.reset();
	vertices.reset();
	indices.reset();
	materials.reset();
	materialIds.reset();
	instances.reset();

	for (auto& image : textures) {
		image.reset();
	}
	*/
}

void DeviceScene::initDescriptor() {
	zvk::DescriptorWrite update;

	update.add(resourceDescLayout.get(), resourceDescSet, 0, zvk::Descriptor::makeImageDescriptorArray(textures));
	update.add(resourceDescLayout.get(), resourceDescSet, 1, zvk::Descriptor::makeBufferInfo(materials.get()));
	update.add(resourceDescLayout.get(), resourceDescSet, 2, zvk::Descriptor::makeBufferInfo(materialIds.get()));
	update.add(resourceDescLayout.get(), resourceDescSet, 3, zvk::Descriptor::makeBufferInfo(vertices.get()));
	update.add(resourceDescLayout.get(), resourceDescSet, 4, zvk::Descriptor::makeBufferInfo(indices.get()));
	update.add(resourceDescLayout.get(), resourceDescSet, 5, zvk::Descriptor::makeBufferInfo(instances.get()));
	update.add(resourceDescLayout.get(), resourceDescSet, 6, zvk::Descriptor::makeBufferInfo(lightInstances.get()));
	update.add(resourceDescLayout.get(), resourceDescSet, 7, zvk::Descriptor::makeBufferInfo(lightSampleTable.get()));

	update.add(rayTracingDescLayout.get(), rayTracingDescSet, 0, vk::WriteDescriptorSetAccelerationStructureKHR(topAccelStructure->structure));

	mCtx->device.updateDescriptorSets(update.writes, {});
}

void DeviceScene::createBufferAndImages(const Scene& scene, zvk::QueueIdx queueIdx) {
	numVertices = scene.resource.vertices().size();
	numIndices = scene.resource.indices().size();
	numMaterials = scene.resource.materials().size();
	numTriangles = numIndices / 3;

	auto RTBuildFlags = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
		vk::BufferUsageFlagBits::eShaderDeviceAddress;

	vertices = zvk::Memory::createBufferFromHost(
		mCtx, queueIdx, scene.resource.vertices().data(), zvk::sizeOf(scene.resource.vertices()),
		vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | RTBuildFlags,
		vk::MemoryAllocateFlagBits::eDeviceAddress
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, vertices->buffer, "vertices");

	indices = zvk::Memory::createBufferFromHost(
		mCtx, queueIdx, scene.resource.indices().data(), zvk::sizeOf(scene.resource.indices()),
		vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | RTBuildFlags,
		vk::MemoryAllocateFlagBits::eDeviceAddress
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, indices->buffer, "indices");

	materials = zvk::Memory::createBufferFromHost(
		mCtx, queueIdx, scene.resource.materials().data(), zvk::sizeOf(scene.resource.materials()),
		vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryAllocateFlagBits::eDeviceAddress
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, materials->buffer, "materials");

	materialIds = zvk::Memory::createBufferFromHost(
		mCtx, queueIdx, scene.resource.materialIndices().data(), zvk::sizeOf(scene.resource.materialIndices()),
		vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryAllocateFlagBits::eDeviceAddress
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, materialIds->buffer, "materialIds");

	instances = zvk::Memory::createBufferFromHost(
		mCtx, queueIdx, scene.objectInstances.data(), zvk::sizeOf(scene.objectInstances),
		vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryAllocateFlagBits::eDeviceAddress
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, instances->buffer, "instances");

	lightInstances = zvk::Memory::createBufferFromHost(
		mCtx, queueIdx, scene.lightInstances.data(), zvk::sizeOf(scene.lightInstances),
		vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryAllocateFlagBits::eDeviceAddress
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, lightInstances->buffer, "lightInstances");

	lightSampleTable = zvk::Memory::createBufferFromHost(
		mCtx, queueIdx, scene.lightSampleTable.binomDistribs.data(), zvk::sizeOf(scene.lightSampleTable.binomDistribs),
		vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryAllocateFlagBits::eDeviceAddress
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, lightSampleTable->buffer, "lightSampleTable");

	auto images = scene.resource.imagePool();

	// Load one extra texture to ensure the array is not empty
	auto extImage = zvk::HostImage::createFromFile("res/texture.jpg", zvk::HostImageType::Int8, 4);
	images.push_back(extImage);

	for (auto hostImage : images) {
		auto image = zvk::Memory::createTexture2D(
			mCtx, queueIdx, hostImage,
			vk::ImageTiling::eOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		image->createSampler(vk::Filter::eLinear);
		textures.push_back(std::move(image));
	}
	delete extImage;
}

void DeviceScene::createAccelerationStructure(const Scene& scene, zvk::QueueIdx queueIdx) {
	for (auto model : scene.resource.uniqueModelInstances()) {
		auto firstMesh = scene.resource.meshInstances()[model->meshOffset()];

		uint64_t indexOffset = firstMesh.indexOffset * sizeof(uint32_t);

		zvk::AccelerationStructureTriangleMesh meshData{
			.vertexAddress = vertices->address(),
			.indexAddress = indices->address() + indexOffset,
			.vertexStride = sizeof(MeshVertex),
			.vertexFormat = vk::Format::eR32G32B32Sfloat,
			.indexType = vk::IndexType::eUint32,
			.maxVertex = model->numVertices(),
			.numIndices = model->numIndices(),
			.indexOffset = firstMesh.indexOffset
		};

		auto BLAS = std::make_unique<zvk::AccelerationStructure>(
			mCtx, queueIdx, meshData, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
		);
		zvk::DebugUtils::nameVkObject(mCtx->device, BLAS->structure, "BLAS_" + model->name());

		meshAccelStructures.push_back(std::move(BLAS));
	}
	std::vector<vk::AccelerationStructureInstanceKHR> instances;

	for (uint32_t i = 0; i < scene.resource.modelInstances().size(); i++) {
		auto modelInstance = scene.resource.modelInstances()[i];

		vk::TransformMatrixKHR transform;
		glm::mat4 matrix = glm::transpose(modelInstance->modelMatrix());
		memcpy(&transform, &matrix, 12 * sizeof(float));

		instances.push_back(
			vk::AccelerationStructureInstanceKHR()
			.setTransform(transform)
			.setInstanceCustomIndex(i)
			.setMask(0xff)
			.setInstanceShaderBindingTableRecordOffset(0)
			.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable)
			.setAccelerationStructureReference(meshAccelStructures[modelInstance->refId()]->address)
		);
	}

	topAccelStructure = std::make_unique<zvk::AccelerationStructure>(
		mCtx, queueIdx, instances, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, topAccelStructure->structure, "TLAS");
}

void DeviceScene::createDescriptor() {
	std::vector<vk::DescriptorSetLayoutBinding> resourceBindings = {
		zvk::Descriptor::makeBinding(
			0, vk::DescriptorType::eCombinedImageSampler,
			vk::ShaderStageFlagBits::eFragment | RayTracingShaderStageFlags,
			textures.size()
		),
		zvk::Descriptor::makeBinding(
			1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | RayTracingShaderStageFlags
		),
		zvk::Descriptor::makeBinding(
			2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex | RayTracingShaderStageFlags
		),
		zvk::Descriptor::makeBinding(
			3, vk::DescriptorType::eStorageBuffer, RayTracingShaderStageFlags
		),
		zvk::Descriptor::makeBinding(
			4, vk::DescriptorType::eStorageBuffer, RayTracingShaderStageFlags
		),
		zvk::Descriptor::makeBinding(
			5, vk::DescriptorType::eStorageBuffer, RayTracingShaderStageFlags
		),
		zvk::Descriptor::makeBinding(
			6, vk::DescriptorType::eStorageBuffer, RayTracingShaderStageFlags
		),
		zvk::Descriptor::makeBinding(
			7, vk::DescriptorType::eStorageBuffer, RayTracingShaderStageFlags
		),
	};

	std::vector<vk::DescriptorSetLayoutBinding> accelStructBindings = {
		zvk::Descriptor::makeBinding(0, vk::DescriptorType::eAccelerationStructureKHR, RayTracingShaderStageFlags)
	};

	resourceDescLayout = std::make_unique<zvk::DescriptorSetLayout>(mCtx, resourceBindings);
	rayTracingDescLayout = std::make_unique<zvk::DescriptorSetLayout>(mCtx, accelStructBindings);

	mDescriptorPool = std::make_unique<zvk::DescriptorPool>(
		mCtx, zvk::DescriptorLayoutArray{ resourceDescLayout.get(), rayTracingDescLayout.get() }, 1,
		vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind
	);

	resourceDescSet = mDescriptorPool->allocDescriptorSet(resourceDescLayout->layout);
	zvk::DebugUtils::nameVkObject(mCtx->device, resourceDescSet, "resourceDescSet");

	rayTracingDescSet = mDescriptorPool->allocDescriptorSet(rayTracingDescLayout->layout);
	zvk::DebugUtils::nameVkObject(mCtx->device, rayTracingDescSet, "rayTracingDescSet");
}
