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

struct GBufferDrawParam {
	std140(glm::mat4, model);
	std140(glm::mat4, modelInvT);
	std140(int32_t, matIdx);
	std140(int32_t, pad0);
	std140(int32_t, pad1);
	std140(int32_t, pad2);
};

class GBufferPass : public zvk::BaseVkObject {
	constexpr static vk::Format GBufferAFormat = vk::Format::eR32G32B32A32Uint;
	constexpr static vk::Format GBufferBFormat = vk::Format::eR32G32B32A32Uint;
	constexpr static vk::Format DepthStencilFormat = vk::Format::eD32Sfloat;

public:
	GBufferPass(
		const zvk::Context* ctx, vk::Extent2D extent, const Resource& resource,
		vk::ImageLayout outLayout = vk::ImageLayout::eShaderReadOnlyOptimal);

	~GBufferPass() { destroy(); }
	void destroy();

	vk::DescriptorSetLayout descSetLayout() const { return mDrawParamDescLayout->layout; }

	void createPipeline(
		vk::Extent2D extent, zvk::ShaderManager* shaderManager,
		const std::vector<vk::DescriptorSetLayout>& descLayouts);

	void render(
		vk::CommandBuffer cmd, vk::Extent2D extent,
		vk::DescriptorSet cameraDescSet, vk::DescriptorSet resourceDescSet,
		vk::Buffer vertexBuffer, vk::Buffer indexBuffer, uint32_t offset, uint32_t count);

	void initDescriptor();

	void swap();
	void recreateFrame(vk::Extent2D extent);

private:
	void createDrawBuffer(const Resource& resource);
	void createResource(vk::Extent2D extent);
	void createRenderPass(vk::ImageLayout outLayout);
	void createFramebuffer(vk::Extent2D extent);
	void createDescriptor();

	void destroyFrame();

public:
	zvk::Image* GBufferA[2] = { nullptr };
	zvk::Image* GBufferB[2] = { nullptr };
	vk::Framebuffer framebuffer[2];

private:
	vk::Pipeline mPipeline;
	vk::RenderPass mRenderPass;
	vk::PipelineLayout mPipelineLayout;

	std::vector<GBufferDrawParam> mDrawParams;
	zvk::Buffer* mDrawCommandBuffer;
	zvk::Buffer* mDrawParamBuffer;

	zvk::Image* mDepthStencil[2] = { nullptr };

	zvk::DescriptorPool* mDescriptorPool = nullptr;
	zvk::DescriptorSetLayout* mDrawParamDescLayout = nullptr;
	vk::DescriptorSet mDrawParamDescSet;

	const bool mMultiDrawSupport;
};