#pragma once

//#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#undef min
#undef max

#include <algorithm>
#include <iostream>
#include <optional>
#include <limits>
#include <set>

#include "zvk.h"
#include "Scene.h"

class PostProcPassComp : public zvk::BaseVkObject {
public:
	struct PushConstant {
		glm::ivec2 frameSize;
		uint32_t toneMapping;
	};

	PostProcPassComp(const zvk::Context* ctx, const zvk::Swapchain* swapchain);
	~PostProcPassComp() { destroy(); }
	void destroy();

	vk::DescriptorSetLayout descSetLayout() const { return mDescriptorSetLayout->layout; }

	void createPipeline(zvk::ShaderManager* shaderManager, const std::vector<vk::DescriptorSetLayout>& descLayouts);

	void render(vk::CommandBuffer cmd, vk::Extent2D extent, uint32_t imageIdx);
	void updateDescriptor(zvk::Image* inImage[2], const zvk::Swapchain* swapchain);
	void swap();

private:
	void createDescriptor(uint32_t nSwapchainImages);

public:
	uint32_t toneMapping = 0;

private:
	vk::Pipeline mPipeline;
	vk::PipelineLayout mPipelineLayout;

	zvk::DescriptorSetLayout* mDescriptorSetLayout = nullptr;
	zvk::DescriptorPool* mDescriptorPool = nullptr;
	std::vector<vk::DescriptorSet> mDescriptorSet[2];
};