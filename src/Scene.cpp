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

bool Scene::load(const File::path& path) {
	Log::bracketLine<0>("Scene " + path.generic_string());
	//clear();

	pugi::xml_document doc;
	doc.load_file(path.generic_string().c_str());
	auto scene = doc.child("scene");

	if (!scene) {
		return false;
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
	return true;
}