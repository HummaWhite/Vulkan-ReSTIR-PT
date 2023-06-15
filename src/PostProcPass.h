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

#include "core/Context.h"
#include "core/Swapchain.h"
#include "core/ShaderManager.h"
#include "core/Memory.h"
#include "core/Descriptor.h"
#include "Scene.h"

#include "util/Error.h"
#include "util/Timer.h"

class PostProcPass : public zvk::BaseVkObject {
public:
	struct PushConstant {
		glm::ivec2 frameSize;
		uint32_t toneMapping;
	};

	PostProcPass(
		const zvk::Context* ctx, const zvk::Swapchain* swapchain, zvk::ShaderManager* shaderManager,
		std::vector<vk::DescriptorSetLayout>& descLayouts);

	~PostProcPass() { destroy(); }
	void destroy();

	vk::DescriptorSetLayout descSetLayout() const { return mDescriptorSetLayout->layout; }

	void render(vk::CommandBuffer cmd, vk::Extent2D extent, uint32_t imageIdx);
	void updateDescriptors(zvk::Image* inImage[2], const zvk::Swapchain* swapchain);
	void swap();

private:
	void createPipeline(zvk::ShaderManager* shaderManager, std::vector<vk::DescriptorSetLayout>& descLayouts);
	void createDescriptors(uint32_t nSwapchainImages);

public:
	uint32_t toneMapping = 0;

private:
	vk::Pipeline mPipeline;
	vk::PipelineLayout mPipelineLayout;

	zvk::DescriptorSetLayout* mDescriptorSetLayout = nullptr;
	zvk::DescriptorPool* mDescriptorPool = nullptr;
	std::vector<vk::DescriptorSet> mDescriptorSet[2];
};