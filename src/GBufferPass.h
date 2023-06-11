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

class GBufferPass : public zvk::BaseVkObject {
	constexpr static vk::Format DepthNormalFormat = vk::Format::eR32G32B32A32Sfloat;
	constexpr static vk::Format AlbedoMatIdxFormat = vk::Format::eR32G32B32A32Sfloat;
	constexpr static vk::Format DepthStencilFormat = vk::Format::eR32Sfloat;

public:
	GBufferPass(const zvk::Context* ctx, vk::Extent2D extent, zvk::ShaderManager* shaderManager);
	~GBufferPass() { destroy(); }
	void destroy();

	void render(
		vk::CommandBuffer cmd, vk::Buffer vertexBuffer,
		vk::Buffer indexBuffer, uint32_t indexOffset, uint32_t indexCount, vk::Extent2D extent);

	void updateDescriptors(const zvk::Buffer* uniforms, const zvk::Image* images);

	void swap();
	void recreateFrames(vk::Extent2D extent);

private:
	void createResources(vk::Extent2D extent);
	void createRenderPass();
	void createFramebuffer(vk::Extent2D extent);
	void createPipeline(vk::Extent2D extent, zvk::ShaderManager* shaderManager);
	void createDescriptors();

	void destroyFrames();

public:
	zvk::Image* depthNormal[2] = { nullptr };
	zvk::Image* albedoMatIdx[2] = { nullptr };
	vk::Framebuffer framebuffer[2];

private:
	vk::Pipeline mPipeline;
	vk::RenderPass mRenderPass;
	vk::PipelineLayout mPipelineLayout;

	zvk::Image* mDepthStencil[2] = { nullptr };

	zvk::DescriptorSetLayout* mDescriptorSetLayout = nullptr;
	zvk::DescriptorPool* mDescriptorPool = nullptr;
	vk::DescriptorSet mDescriptorSet[2];
};