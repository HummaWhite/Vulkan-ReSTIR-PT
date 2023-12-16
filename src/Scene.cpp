#include "Scene.h"
#include "util/Error.h"
#include "shader/HostDevice.h"

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

std::pair<ModelInstance*, glm::vec3> Scene::loadModelInstance(const pugi::xml_node& modelNode) {
	glm::vec3 power(0.f);
	bool isLight = false;

	if (std::string(modelNode.attribute("type").as_string()) == "light") {
		std::stringstream radStr(modelNode.child("radiance").attribute("value").as_string());
		radStr >> power.r >> power.g >> power.b;
		isLight = true;
	}

	auto modelPath = modelNode.attribute("path").as_string();
	File::path absolutePath = path.parent_path() / modelPath;
	auto model = resource.openModelInstance(absolutePath, isLight, glm::vec3(0.0f));

	std::string name(modelNode.attribute("name").as_string());
	model->setName(name);
	model->setPath(absolutePath.c_str());

	auto transNode = modelNode.child("transform");
	auto [pos, scale, rot] = loadTransform(transNode);

	model->setPos(pos);
	model->setScale(scale.x, scale.y, scale.z);
	model->setRotation(rot);

	if (!isLight && modelNode.child("material")) {
		auto matNode = modelNode.child("material");
		auto material = loadMaterialNoBaseColor(matNode);

		if (material) {
			uint32_t textureIdx = InvalidResourceIdx;
			glm::vec3 baseColor = glm::vec3(1.0);

			if (auto baseColorNode = matNode.child("baseColor")) {
				if (auto valAttrib = baseColorNode.attribute("value")) {
					std::stringstream ss(valAttrib.as_string());
					ss >> baseColor.r >> baseColor.g >> baseColor.b;
				}
				if (auto imgAttrib = baseColorNode.attribute("image")) {
					auto imagePath = path.parent_path() / imgAttrib.as_string();

					auto filter = zvk::HostImageFilter::Linear;

					if (auto filterAttrib = baseColorNode.attribute("filter")) {
						if (std::string(filterAttrib.as_string()) == "nearest") {
							filter = zvk::HostImageFilter::Nearest;
						}
					}
					auto loadedTexIdx = resource.addImage(imagePath, zvk::HostImageType::Int8, filter);

					if (loadedTexIdx) {
						textureIdx = *loadedTexIdx;
						Log::line<2>("Albedo texture " + imagePath.generic_string());
					}
				}
			}

			for (uint32_t i = 0; i < model->numMeshes(); i++) {
				uint32_t materialIdx = resource.meshInstances[Resource::Object][i + model->meshOffset()].materialIdx;
				material->textureIdx = (textureIdx != InvalidResourceIdx) ? textureIdx : resource.materials[materialIdx].textureIdx;
				material->baseColor = baseColor;
				resource.materials[materialIdx] = *material;
			}
		}
	}
	return { model, power };
}

void Scene::load(const File::path& path) {
	//clear();
	this->path = path;
	Log::line<0>("Scene " + path.generic_string());

	pugi::xml_document doc;
	doc.load_file(path.generic_string().c_str());
	loadXML(doc.child("scene"));

	buildLightDataStructure();

	Log::line<1>("Statistics");
	Log::line<2>("Objects");
	Log::line<3>("Vertices = " + std::to_string(resource.vertices[Resource::Object].size()));
	Log::line<3>("Indices = " + std::to_string(resource.indices[Resource::Object].size()));
	Log::line<3>("Mesh instances = " + std::to_string(resource.meshInstances[Resource::Object].size()));
	Log::line<3>("Model instances = " + std::to_string(resource.modelInstances[Resource::Object].size()));
	Log::line<3>("Materials = " + std::to_string(resource.materials.size()));
	Log::newLine();
}

void Scene::clear() {
	resource.destroy();
	objectInstances.clear();
	triangleLights.clear();
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
	glm::vec3 pos, angleOrLookAt;
	std::string posStr(cameraNode.child("position").attribute("value").as_string());
	std::stringstream posSS(posStr);
	posSS >> pos.x >> pos.y >> pos.z;
	camera.setPos(pos);

	std::string angleOrLookAtStr;

	if (cameraNode.child("angle")) {
		angleOrLookAtStr = cameraNode.child("angle").attribute("value").as_string();
		std::stringstream ss(angleOrLookAtStr);
		ss >> angleOrLookAt.x >> angleOrLookAt.y >> angleOrLookAt.z;
		camera.setAngle(angleOrLookAt);
		angleOrLookAtStr = "Angle " + angleOrLookAtStr;
	}
	else {
		angleOrLookAtStr = cameraNode.child("lookAt").attribute("value").as_string();
		std::stringstream ss(angleOrLookAtStr);
		ss >> angleOrLookAt.x >> angleOrLookAt.y >> angleOrLookAt.z;
		camera.lookAt(angleOrLookAt);
		angleOrLookAtStr = "LookAt " + angleOrLookAtStr;
	}

	camera.setFOV(cameraNode.child("fov").attribute("value").as_float());
	camera.setLensRadius(cameraNode.child("lensRadius").attribute("value").as_float());
	camera.setFocalDist(cameraNode.child("focalDistance").attribute("value").as_float());

	Log::line<1>("Camera " + std::string(cameraNode.attribute("type").as_string()));
	Log::line<2>("Position " + posStr);
	Log::line<2>(angleOrLookAtStr);
	Log::line<2>("FOV " + std::to_string(camera.FOV()));
	Log::line<2>("LensRadius " + std::to_string(camera.lensRadius()));
	Log::line<2>("FocalDistance " + std::to_string(camera.focalDist()));
}

void Scene::loadModels(pugi::xml_node modelNode) {
	std::vector<std::pair<ModelInstance*, glm::vec3>> instances;
	uint32_t totalTriangleCount = 0;
	uint32_t lightTriangleCount = 0;

	for (auto instance = modelNode.first_child(); instance; instance = instance.next_sibling()) {
		auto instanceAndPower = loadModelInstance(instance);
		instances.push_back(instanceAndPower);

		if (glm::length(instanceAndPower.second) > 0) {
			lightTriangleCount += instanceAndPower.first->numIndices() / 3;
		}
		totalTriangleCount += instanceAndPower.first->numIndices() / 3;
	}
	triangleLights.resize(lightTriangleCount);

	uint32_t lightTriangleOffset = 0;

	for (const auto& [modelInstance, power] : instances) {
		glm::mat4 transform = modelInstance->modelMatrix();
		glm::mat4 transformInv = glm::inverse(transform);
		glm::mat4 transformInvT = glm::transpose(transformInv);

		bool isLight = glm::length(power) > 0;

		if (isLight) {
			const auto& beginMeshInstance = resource.meshInstances[Resource::Light][modelInstance->meshOffset()];
			uint32_t indexOffset = beginMeshInstance.indexOffset;
			uint32_t indexCount = modelInstance->numIndices();
			uint32_t triangleCount = indexCount / 3;

			auto transform = [&](const glm::vec3& pos) {
				return glm::vec3(modelInstance->modelMatrix() * glm::vec4(pos, 1.f));
			};

			uint32_t maxThreads = std::min(triangleCount, std::thread::hardware_concurrency());
			std::vector<float> sumAreas(maxThreads, 0.f);

			auto fillLightTriangle = [&](uint32_t begin, uint32_t end, uint32_t id) {
				for (uint32_t i = begin; i < end; i++) {
					TriangleLight tri{};

					uint32_t i0 = resource.indices[Resource::Light][indexOffset + i * 3 + 0];
					uint32_t i1 = resource.indices[Resource::Light][indexOffset + i * 3 + 1];
					uint32_t i2 = resource.indices[Resource::Light][indexOffset + i * 3 + 2];

					tri.v0 = transform(resource.vertices[Resource::Light][i0].pos);
					tri.v1 = transform(resource.vertices[Resource::Light][i1].pos);
					tri.v2 = transform(resource.vertices[Resource::Light][i2].pos);

					glm::vec3 n = glm::cross(tri.v1 - tri.v0, tri.v2 - tri.v0);
					tri.area = .5f * glm::length(n);
					n = glm::normalize(n);

					tri.nx = n.x;
					tri.ny = n.y;
					tri.nz = n.z;

					sumAreas[id] += tri.area;
					triangleLights[lightTriangleOffset + i] = tri;
				}
			};

			std::vector<std::thread> threads(maxThreads);

			for (uint32_t i = 0; i < maxThreads; i++) {
				uint32_t begin = i * (triangleCount / maxThreads);
				uint32_t end = std::min((i + 1) * (triangleCount / maxThreads), triangleCount);
				threads[i] = std::thread(fillLightTriangle, begin, end, i);
			}
			for (auto& thread : threads) {
				thread.join();
			}
			float sumArea = 0.f;

			for (auto i : sumAreas) {
				sumArea += i;
			}
			
			auto divideArea = [&](uint32_t begin, uint32_t end) {
				for (uint32_t i = begin; i < end; i++) {
					triangleLights[lightTriangleOffset + i].radiance = power / static_cast<float>(sumArea);
				}
			};

			for (uint32_t i = 0; i < maxThreads; i++) {
				uint32_t begin = i * (triangleCount / maxThreads);
				uint32_t end = std::min((i + 1) * (triangleCount / maxThreads), triangleCount);
				threads[i] = std::thread(divideArea, begin, end);
			}
			for (auto& thread : threads) {
				thread.join();
			}
			
			lightTriangleOffset += triangleCount;
		}
		else {
			objectInstances.push_back(ObjectInstance{
				.transform = transform,
				.transformInv = transformInv,
				.transformInvT = transformInvT,
				.radiance = power,
				.indexOffset = resource.meshInstances[Resource::Object][modelInstance->meshOffset()].indexOffset,
				.indexCount = modelInstance->mNumIndices
			});
		}
	}
}

void Scene::loadEnvironmentMap(pugi::xml_node envMapNode) {
	//envMap = EnvironmentMap::create(scene.child("envMap").attribute("path").as_string());
}

void Scene::buildLightDataStructure() {
	Log::line<1>("Light Sample Table");
	std::vector<float> powerDistrib(triangleLights.size());

	float sumArea = 0.f;

	for (uint32_t i = 0; i < triangleLights.size(); i++) {
		powerDistrib[i] = luminance(triangleLights[i].radiance * triangleLights[i].area);
	}
	lightSampleTable.build(powerDistrib);

	Log::line<2>("Sum = " + std::to_string(lightSampleTable.binomDistribs[0].prob));
	Log::line<2>("Num = " + std::to_string(lightSampleTable.binomDistribs[0].failId));
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
	zvk::DescriptorWrite update(mCtx);

	update.add(resourceDescLayout.get(), resourceDescSet, 0, zvk::Descriptor::makeImageDescriptorArray(textures));
	update.add(resourceDescLayout.get(), resourceDescSet, 1, zvk::Descriptor::makeBuffer(materials.get()));
	update.add(resourceDescLayout.get(), resourceDescSet, 2, zvk::Descriptor::makeBuffer(materialIds.get()));
	update.add(resourceDescLayout.get(), resourceDescSet, 3, zvk::Descriptor::makeBuffer(vertices.get()));
	update.add(resourceDescLayout.get(), resourceDescSet, 4, zvk::Descriptor::makeBuffer(indices.get()));
	update.add(resourceDescLayout.get(), resourceDescSet, 5, zvk::Descriptor::makeBuffer(instances.get()));
	update.add(resourceDescLayout.get(), resourceDescSet, 6, zvk::Descriptor::makeBuffer(triangleLights.get()));
	update.add(resourceDescLayout.get(), resourceDescSet, 7, zvk::Descriptor::makeBuffer(lightSampleTable.get()));

	update.add(rayTracingDescLayout.get(), rayTracingDescSet, 0, vk::WriteDescriptorSetAccelerationStructureKHR(topAccelStructure->structure));

	update.flush();
}

void DeviceScene::createBufferAndImages(const Scene& scene, zvk::QueueIdx queueIdx) {
	numVertices = static_cast<uint32_t>(scene.resource.vertices[Resource::Object].size());
	numIndices = static_cast<uint32_t>(scene.resource.indices[Resource::Object].size());
	numMaterials = static_cast<uint32_t>(scene.resource.materials.size());
	numTriangles = static_cast<uint32_t>(numIndices / 3);
	numTriangleLights = static_cast<uint32_t>(scene.triangleLights.size());

	auto RTBuildFlags = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
		vk::BufferUsageFlagBits::eShaderDeviceAddress;

	vertices = zvk::Memory::createBufferFromHost(
		mCtx, queueIdx, scene.resource.vertices[Resource::Object].data(), zvk::sizeOf(scene.resource.vertices[Resource::Object]),
		vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | RTBuildFlags,
		vk::MemoryAllocateFlagBits::eDeviceAddress
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, vertices->buffer, "vertices");

	indices = zvk::Memory::createBufferFromHost(
		mCtx, queueIdx, scene.resource.indices[Resource::Object].data(), zvk::sizeOf(scene.resource.indices[Resource::Object]),
		vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | RTBuildFlags,
		vk::MemoryAllocateFlagBits::eDeviceAddress
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, indices->buffer, "indices");

	materials = zvk::Memory::createBufferFromHost(
		mCtx, queueIdx, scene.resource.materials.data(), zvk::sizeOf(scene.resource.materials),
		vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryAllocateFlagBits::eDeviceAddress
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, materials->buffer, "materials");

	materialIds = zvk::Memory::createBufferFromHost(
		mCtx, queueIdx, scene.resource.materialIndices.data(), zvk::sizeOf(scene.resource.materialIndices),
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

	triangleLights = zvk::Memory::createBufferFromHost(
		mCtx, queueIdx, scene.triangleLights.data(), zvk::sizeOf(scene.triangleLights),
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress |
			vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
		vk::MemoryAllocateFlagBits::eDeviceAddress
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, triangleLights->buffer, "triangleLights");

	lightSampleTable = zvk::Memory::createBufferFromHost(
		mCtx, queueIdx, scene.lightSampleTable.binomDistribs.data(), zvk::sizeOf(scene.lightSampleTable.binomDistribs),
		vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryAllocateFlagBits::eDeviceAddress
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, lightSampleTable->buffer, "lightSampleTable");

	auto images = scene.resource.imagePool();

	// Load one extra texture to ensure the array is not empty
	auto extImage = zvk::HostImage::createFromFile("res/texture.jpg", zvk::HostImageType::Int8, zvk::HostImageFilter::Nearest, 4);
	images.push_back(extImage);

	for (auto hostImage : images) {
		auto image = zvk::Memory::createTexture2D(
			mCtx, queueIdx, hostImage,
			vk::ImageTiling::eOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		image->createSampler(hostImage->filter == zvk::HostImageFilter::Linear ? vk::Filter::eLinear : vk::Filter::eNearest);
		textures.push_back(std::move(image));
	}
	delete extImage;
}

void DeviceScene::createAccelerationStructure(const Scene& scene, zvk::QueueIdx queueIdx) {
	Log::line<0>("Creating acceleration structures");

	std::unique_ptr<zvk::Buffer> lightIndicesBuf;
	std::vector<vk::AccelerationStructureInstanceKHR> instances;

	if (/* build light AS */ true) {
		std::vector<uint32_t> lightIndices(numTriangleLights * 3);

		for (uint32_t i = 0; i < numTriangleLights; i++) {
			lightIndices[3 * i + 0] = 4 * i + 0;
			lightIndices[3 * i + 1] = 4 * i + 1;
			lightIndices[3 * i + 2] = 4 * i + 2;
		}
		lightIndicesBuf = zvk::Memory::createBufferFromHost(
			mCtx, queueIdx, lightIndices.data(), zvk::sizeOf(lightIndices),
			vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
				vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
				vk::BufferUsageFlagBits::eShaderDeviceAddress,
			vk::MemoryAllocateFlagBits::eDeviceAddress
		);

		zvk::AccelerationStructureTriangleMesh meshData {
			.vertexAddress = triangleLights->address(),
			.indexAddress = lightIndicesBuf->address(),
			.vertexStride = sizeof(glm::vec4),
			.vertexFormat = vk::Format::eR32G32B32Sfloat,
			.indexType = vk::IndexType::eUint32,
			.maxVertex = numTriangleLights * 3,
			.numIndices = numTriangleLights * 3,
			.indexOffset = 0
		};

		auto lightBLAS = std::make_unique<zvk::AccelerationStructure>(
			mCtx, queueIdx, meshData, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
		);
		zvk::DebugUtils::nameVkObject(mCtx->device, lightBLAS->structure, "lightBLAS");
		meshAccelStructures.push_back(std::move(lightBLAS));

		vk::TransformMatrixKHR transform;
		glm::mat4 matrix(1.f);
		memcpy(&transform, &matrix, 12 * sizeof(float));

		instances.push_back(
			vk::AccelerationStructureInstanceKHR()
				.setTransform(transform)
				.setInstanceCustomIndex(0)
				.setMask(0xff)
				.setInstanceShaderBindingTableRecordOffset(0)
				.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable)
				.setAccelerationStructureReference(meshAccelStructures[0]->address)
		);
	}

	for (auto model : scene.resource.uniqueModelInstances[Resource::Object]) {
		auto firstMesh = scene.resource.meshInstances[Resource::Object][model->meshOffset()];

		uint64_t indexOffset = firstMesh.indexOffset * sizeof(uint32_t);

		zvk::AccelerationStructureTriangleMesh meshData {
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
		zvk::DebugUtils::nameVkObject(mCtx->device, BLAS->structure, "objectBLAS_" + model->name());
		meshAccelStructures.push_back(std::move(BLAS));
	}

	for (uint32_t i = 0; i < scene.resource.modelInstances[Resource::Object].size(); i++) {
		auto modelInstance = scene.resource.modelInstances[Resource::Object][i];

		vk::TransformMatrixKHR transform;
		glm::mat4 matrix = glm::transpose(modelInstance->modelMatrix());
		memcpy(&transform, &matrix, 12 * sizeof(float));

		instances.push_back(
			vk::AccelerationStructureInstanceKHR()
				.setTransform(transform)
				.setInstanceCustomIndex(i + 1)
				.setMask(0xff)
				.setInstanceShaderBindingTableRecordOffset(0)
				.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable)
				.setAccelerationStructureReference(meshAccelStructures[modelInstance->refId() + 1]->address)
		);
	}

	topAccelStructure = std::make_unique<zvk::AccelerationStructure>(
		mCtx, queueIdx, instances, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, topAccelStructure->structure, "TLAS");
}

void DeviceScene::createDescriptor() {
	const vk::ShaderStageFlags rayTracingStageFlags = RayPipelineShaderStageFlags | RayQueryShaderStageFlags;
	const vk::ShaderStageFlags gbufferStageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

	std::vector<vk::DescriptorSetLayoutBinding> resourceBindings = {
		zvk::Descriptor::makeBinding(
			0, vk::DescriptorType::eCombinedImageSampler, gbufferStageFlags | rayTracingStageFlags,
			static_cast<uint32_t>(textures.size())
		),
		zvk::Descriptor::makeBinding(
			1, vk::DescriptorType::eStorageBuffer, gbufferStageFlags | rayTracingStageFlags
		),
		zvk::Descriptor::makeBinding(
			2, vk::DescriptorType::eStorageBuffer, gbufferStageFlags | rayTracingStageFlags
		),
		zvk::Descriptor::makeBinding(
			3, vk::DescriptorType::eStorageBuffer, rayTracingStageFlags
		),
		zvk::Descriptor::makeBinding(
			4, vk::DescriptorType::eStorageBuffer, rayTracingStageFlags
		),
		zvk::Descriptor::makeBinding(
			5, vk::DescriptorType::eStorageBuffer, gbufferStageFlags | rayTracingStageFlags
		),
		zvk::Descriptor::makeBinding(
			6, vk::DescriptorType::eStorageBuffer, rayTracingStageFlags
		),
		zvk::Descriptor::makeBinding(
			7, vk::DescriptorType::eStorageBuffer, rayTracingStageFlags
		),
	};

	std::vector<vk::DescriptorSetLayoutBinding> accelStructBindings = {
		zvk::Descriptor::makeBinding(0, vk::DescriptorType::eAccelerationStructureKHR, rayTracingStageFlags)
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
