#pragma once

#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/HostImage.h"
#include "Camera.h"
#include "Model.h"
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
	Resource resource;
	Camera camera;
};