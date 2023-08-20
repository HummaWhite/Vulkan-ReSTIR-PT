#pragma once

#include <iostream>
#include <variant>
#include <optional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <pugixml.hpp>

#include "util/NamespaceDecl.h"
#include "core/Alignment.h"

constexpr int InvalidResourceIdx = -1;

struct Material {
	enum { Lambertian = 0, Principled, MetalWorkflow, Dielectric, ThinDielectric, Light };

	std140(glm::vec3, baseColor) = glm::vec3(1.0f);
	std140(uint32_t, type) = Lambertian;

	std140(int32_t, textureIdx) = -InvalidResourceIdx;
	std140(float, metallic) = 0.0f;
	std140(float, roughness) = 1.0f;
	std140(float, ior) = 1.5f;

	std140(float, specular) = 1.0f;
	std140(float, specularTint) = 1.0f;
	std140(float, sheen) = 0.0f;
	std140(float, sheenTint) = 1.0f;

	std140(float, clearcoat) = 0.0f;
	std140(float, clearcoatGloss) = 0.0f;
	std140(float, subsurface) = 0.0f;
	std140(uint32_t, lightIdx) = 0;
};

std::optional<Material> loadMaterial(const pugi::xml_node& node);