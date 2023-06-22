#include "Scene.h"
#include "util/Error.h"

#include <sstream>
#include <pugixml.hpp>

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
		uint32_t materialIdx = resource.mMeshInstances[i + model->offset()].materialIdx;

		auto textureIdx = resource.mMaterials[materialIdx].textureIdx;
		resource.mMaterials[materialIdx] = *material;
		resource.mMaterials[materialIdx].textureIdx = textureIdx;
	}

	return { model, std::nullopt };
}

void Scene::load(const File::path& path) {
	Log::bracketLine<0>("Scene " + path.generic_string());
	//clear();

	pugi::xml_document doc;
	doc.load_file(path.generic_string().c_str());
	auto scene = doc.child("scene");

	if (!scene) {
		throw std::runtime_error("Scene: failed to load");
	}

	{
		auto integrator = scene.child("integrator");
		std::string integStr(integrator.attribute("type").as_string());
		
		auto size = integrator.child("size");
		camera.setFilmSize({ size.attribute("width").as_int(), size.attribute("height").as_int() });

		auto toneMapping = integrator.child("toneMapping");
		// TODO: load toneMapping
		Log::bracketLine<1>("Integrator " + integStr);
	}
	{
		auto samplerNode = scene.child("sampler");
		/*
		sampler = (std::string(samplerNode.attribute("type").as_string()) == "sobol") ? 1 : 0;
		*/
	}
	{
		auto cameraNode = scene.child("camera");

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

		Log::bracketLine<1>("Camera " + std::string(cameraNode.attribute("type").as_string()));
		Log::bracketLine<2>("Position " + posStr);
		Log::bracketLine<2>("Angle " + angleStr);
		Log::bracketLine<2>("FOV " + std::to_string(camera.FOV()));
		Log::bracketLine<2>("LensRadius " + std::to_string(camera.lensRadius()));
		Log::bracketLine<2>("FocalDistance " + std::to_string(camera.focalDist()));
	}
	{
		auto modelInstances = scene.child("modelInstances");
		for (auto instance = modelInstances.first_child(); instance; instance = instance.next_sibling()) {
			auto [model, radiance] = loadModelInstance(instance);

			resource.mModelInstances.push_back(model);
			/*
			if (radiance) {
				addLight(model, radiance.value());
			}
			else {
				addObject(model);
			}
			*/
		}
	}
	{
		//envMap = EnvironmentMap::create(scene.child("envMap").attribute("path").as_string());
	}
	Log::bracketLine<1>("Statistics");
	Log::bracketLine<2>("Vertices = " + std::to_string(resource.vertices().size()));
	Log::bracketLine<2>("Indices = " + std::to_string(resource.indices().size()));
	Log::bracketLine<2>("Mesh instances = " + std::to_string(resource.meshInstances().size()));
	Log::bracketLine<2>("Model instances = " + std::to_string(resource.modelInstances().size()));
	Log::bracketLine<2>("Materials = " + std::to_string(resource.materials().size()));
}

DeviceScene::DeviceScene(const zvk::Context* ctx, const Scene& scene, zvk::QueueIdx queueIdx) :
	BaseVkObject(ctx)
{
	createBufferAndImages(scene, queueIdx);
	createDescriptor();
}

void DeviceScene::destroy() {
	delete cameraDescLayout;
	delete resourceDescLayout;
	delete mDescriptorPool;

	delete camera;
	delete vertices;
	delete indices;
	delete materials;
	delete materialIds;

	for (auto& image : textures) {
		delete image;
	}
}

void DeviceScene::initDescriptor() {
	zvk::DescriptorWrite update;

	update.add(cameraDescLayout, cameraDescSet, 0, vk::DescriptorBufferInfo(camera->buffer, 0, camera->size));
	update.add(resourceDescLayout, resourceDescSet, 0, zvk::Descriptor::makeImageDescriptorArray(textures));
	update.add(resourceDescLayout, resourceDescSet, 1, vk::DescriptorBufferInfo(materials->buffer, 0, materials->size));
	update.add(resourceDescLayout, resourceDescSet, 2, vk::DescriptorBufferInfo(materialIds->buffer, 0, materialIds->size));

	mCtx->device.updateDescriptorSets(update.writes, {});
}

void DeviceScene::createBufferAndImages(const Scene& scene, zvk::QueueIdx queueIdx) {
	numVertices = scene.resource.vertices().size();
	numIndices = scene.resource.indices().size();
	numMaterials = scene.resource.materials().size();

	vertices = zvk::Memory::createBufferFromHost(
		mCtx, queueIdx, scene.resource.vertices().data(), zvk::sizeOf(scene.resource.vertices()),
		vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer
	);

	indices = zvk::Memory::createBufferFromHost(
		mCtx, queueIdx, scene.resource.indices().data(), zvk::sizeOf(scene.resource.indices()),
		vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer
	);

	materials = zvk::Memory::createBufferFromHost(
		mCtx, queueIdx, scene.resource.materials().data(), zvk::sizeOf(scene.resource.materials()),
		vk::BufferUsageFlagBits::eStorageBuffer
	);

	materialIds = zvk::Memory::createBufferFromHost(
		mCtx, queueIdx, scene.resource.materialIndices().data(), zvk::sizeOf(scene.resource.materialIndices()),
		vk::BufferUsageFlagBits::eStorageBuffer
	);

	camera = zvk::Memory::createBuffer(
		mCtx, sizeof(Camera),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);
	camera->mapMemory();

	auto images = scene.resource.imagePool();

	auto extImage = zvk::HostImage::createFromFile("res/texture.jpg", zvk::HostImageType::Int8, 4);
	images.push_back(extImage);

	for (auto hostImage : images) {
		auto image = zvk::Memory::createTexture2D(
			mCtx, queueIdx, hostImage,
			vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
			vk::ImageLayout::eShaderReadOnlyOptimal, vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		image->createSampler(vk::Filter::eLinear);
		textures.push_back(image);
	}
	delete extImage;
}

void DeviceScene::createDescriptor() {
	std::vector<vk::DescriptorSetLayoutBinding> cameraBindings = {
		zvk::Descriptor::makeBinding(
			0, vk::DescriptorType::eUniformBuffer,
			vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute
		)
	};

	std::vector<vk::DescriptorSetLayoutBinding> resourceBindings = {
		zvk::Descriptor::makeBinding(
			0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, textures.size()
		),
		zvk::Descriptor::makeBinding(
			1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
		),
		zvk::Descriptor::makeBinding(
			2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex
		),
	};

	cameraDescLayout = new zvk::DescriptorSetLayout(mCtx, cameraBindings);
	resourceDescLayout = new zvk::DescriptorSetLayout(mCtx, resourceBindings);

	mDescriptorPool = new zvk::DescriptorPool(
		mCtx, { cameraDescLayout, resourceDescLayout }, 1, vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind
	);

	cameraDescSet = mDescriptorPool->allocDescriptorSet(cameraDescLayout->layout);
	resourceDescSet = mDescriptorPool->allocDescriptorSet(resourceDescLayout->layout);
}
