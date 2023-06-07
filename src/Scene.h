#pragma once

#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Camera.h"
#include "Model.h"
#include "HostImage.h"
#include "Resource.h"

class Scene
{
public:
	bool load(const File::path& path);
	//void clear();
	void saveToFile(const File::path& path);

private:
	//void addObject(ModelInstance* object);
	//void addMaterial(const Material& material);
	//void addLight(ModelInstance* light, const glm::vec3& power);

	std::pair<ModelInstance*, std::optional<glm::vec3>> loadModelInstance(const pugi::xml_node& modelNode);

public:
	Resource resource;
	Camera camera;
};