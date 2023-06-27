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

class PostProcPassFrag : public zvk::BaseVkObject {
public:
	struct PushConstant {
		glm::ivec2 frameSize;
		uint32_t toneMapping;
	};

	PostProcPassFrag(const zvk::Context* ctx, const zvk::Swapchain* swapchain);
	~PostProcPassFrag() { destroy(); }
	void destroy();

	void createPipeline(
		zvk::ShaderManager* shaderManager, vk::Extent2D extent,
		const std::vector<vk::DescriptorSetLayout>& descLayouts);

	void render(vk::CommandBuffer cmd, vk::DescriptorSet imageOutDescSet, vk::Extent2D extent, uint32_t imageIdx);
	void recreateFrame(const zvk::Swapchain* swapchain);

private:
	void createResource();
	void createRenderPass();
	void createFramebuffer(const zvk::Swapchain* swapchain);

	void destroyFrame();

public:
	uint32_t toneMapping = 0;
	std::vector<vk::Framebuffer> framebuffers;

private:
	vk::Pipeline mPipeline;
	vk::PipelineLayout mPipelineLayout;
	vk::RenderPass mRenderPass;

	zvk::Buffer* mQuadVertexBuffer = nullptr;
};