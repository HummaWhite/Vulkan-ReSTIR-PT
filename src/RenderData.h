#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <array>

#include "Alignment.h"

struct Vertex {
	constexpr static vk::VertexInputBindingDescription bindingDescription() {
		return vk::VertexInputBindingDescription()
			.setBinding(0)
			.setStride(sizeof(Vertex))
			.setInputRate(vk::VertexInputRate::eVertex);
		// return { 0, sizeof(Vertex), vk::VertexInputRate::eVertex };
	}

	constexpr static std::array<vk::VertexInputAttributeDescription, 2> attributeDescription() {
		return {
			vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(0)
				.setOffset(offsetof(Vertex, pos))
				.setFormat(vk::Format::eR32G32B32Sfloat),
			vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(1)
				.setOffset(offsetof(Vertex, color))
				.setFormat(vk::Format::eR32G32B32Sfloat)
		};
		/*
		return
		{
			{ 0, 0, offsetof(Vertex, pos), vk::Format::eR32G32B32Sfloat) },
			{ 0, 1, offsetof(Vertex, color), vk::Format::eR32G32B32Sfloat) }
		}
		*/
	}

	glm::vec3 pos;
	glm::vec3 color;
};

struct CameraData {
	std140(float, pad0);
	std140(glm::vec3, pad2);

	std140(glm::mat4, model);
	std140(glm::mat4, view);
	std140(glm::mat4, proj);
};

const std::vector<Vertex> VertexData = {
	{ { -0.5f, -0.5f, 0.f }, { 1.f, 0.f, 0.f } },
	{ { 0.5f, -0.5f, 0.f }, { 0.f, 1.f, 0.f } },
	{ { 0.5f, 0.5f, 0.f }, { 0.f, 0.f, 1.f } },
	{ { -0.5f, 0.5f, 0.f }, { 1.f, 1.f, 1.f } }
};

const std::vector<uint32_t> IndexData = {
	0, 1, 2, 2, 3, 0
};