#pragma once

#include <zvk.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader/HostDevice.h"

struct GBufferRenderParam {
	vk::DescriptorSet cameraDescSet;
	vk::DescriptorSet resourceDescSet;
	vk::Buffer vertexBuffer;
	vk::Buffer indexBuffer;
	uint32_t offset;
	uint32_t count;
};

class Resource;

class GBufferPass : public zvk::BaseVkObject {
	constexpr static vk::Format DepthNormalFormat = vk::Format::eR32G32B32A32Sfloat;
	constexpr static vk::Format AlbedoMatIdFormat = vk::Format::eR32G32Uint;
	constexpr static vk::Format MotionVectorFormat = vk::Format::eR16G16Sfloat;
	constexpr static vk::Format DepthStencilFormat = vk::Format::eD32Sfloat;

public:
	GBufferPass(
		const zvk::Context* ctx, vk::Extent2D extent, const Resource& resource,
		vk::ImageLayout outLayout = vk::ImageLayout::eShaderReadOnlyOptimal);

	~GBufferPass() { destroy(); }
	void destroy();

	void createPipeline(
		vk::Extent2D extent, zvk::ShaderManager* shaderManager,
		const std::vector<vk::DescriptorSetLayout>& descLayouts);

	void render(vk::CommandBuffer cmd, vk::Extent2D extent, uint32_t inFlightIdx, uint32_t curFrame, const GBufferRenderParam& param);

	void recreateFrame(vk::Extent2D extent);

private:
	void createDrawBuffer(const Resource& resource);
	void createResource(vk::Extent2D extent);
	void createRenderPass(vk::ImageLayout outLayout);
	void createFramebuffer(vk::Extent2D extent);

	void destroyFrame();

public:
	std::unique_ptr<zvk::Image> depthNormal[NumFramesInFlight][2];
	std::unique_ptr<zvk::Image> albedoMatId[NumFramesInFlight][2];
	std::unique_ptr<zvk::Image> motionVector[NumFramesInFlight];
	vk::Framebuffer framebuffer[NumFramesInFlight][2];

	uint32_t numInstances = 0;

private:
	vk::Pipeline mPipeline;
	vk::RenderPass mRenderPass;
	vk::PipelineLayout mPipelineLayout;

	std::unique_ptr<zvk::Buffer> mIndirectDrawBuffer;
	std::unique_ptr<zvk::Image> mDepthStencil[NumFramesInFlight][2];

	const bool mMultiDrawSupport;
};