#pragma once

#include <iostream>
#include <variant>
#include <optional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <pugixml.hpp>

#include "util/NamespaceDecl.h"
#include "shader/HostDevice.h"

struct Material {
	enum Type {
		Principled = 0, Lambertian, MetalWorkflow, Metal, Dielectric, ThinDielectric, Light
	};

	glm::vec3 baseColor = glm::vec3(1.0f);
	uint32_t type = Lambertian;

	uint32_t textureIdx = InvalidResourceIdx;
	float metallic = 0.0f;
	float roughness = 1.0f;
	float ior = 1.5f;

#if !RESTIR_PT_MATERIAL
	float specular = 1.0f;
	float specularTint = 1.0f;
	float sheen = 0.0f;
	float sheenTint = 1.0f;

	float clearcoat = 0.0f;
	float clearcoatGloss = 0.0f;
	float subsurface = 0.0f;
	uint32_t lightIdx = 0;
#endif
};

std::optional<Material> loadMaterialNoBaseColor(const pugi::xml_node& node);