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
		uint32_t toneMapping;
		uint32_t correctGamma;
		uint32_t noDirect;
		uint32_t noIndirect;
	};

	PostProcPassFrag(const zvk::Context* ctx, const zvk::Swapchain* swapchain);
	~PostProcPassFrag() { destroy(); }
	void destroy();

	void createPipeline(
		zvk::ShaderManager* shaderManager, vk::Extent2D extent,
		const std::vector<vk::DescriptorSetLayout>& descLayouts);

	void render(
		vk::CommandBuffer cmd, uint32_t imageIdx,
		vk::DescriptorSet rayImageDescSet, vk::Extent2D extent, PushConstant param);

	void recreateFrame(const zvk::Swapchain* swapchain);

private:
	void createResource();
	void createRenderPass();
	void createFramebuffer(const zvk::Swapchain* swapchain);

	void destroyFrame();

public:
	std::vector<vk::Framebuffer> framebuffers;

private:
	vk::Pipeline mPipeline;
	vk::PipelineLayout mPipelineLayout;
	vk::RenderPass mRenderPass;

	std::unique_ptr<zvk::Buffer> mQuadVertexBuffer;
};