#pragma once

#include "zvk.h"
#include "Resource.h"
#include "shader/HostDevice.h"

struct GBufferDrawParam {
	std140(glm::mat4, model);
	std140(glm::mat4, modelInvT);
	std140(int32_t, matIdx);
	std140(int32_t, meshIdx);
	std140(int32_t, pad1);
	std140(int32_t, pad2);
};

struct GBufferRenderParam {
	vk::DescriptorSet cameraDescSet;
	vk::DescriptorSet resourceDescSet;
	vk::Buffer vertexBuffer;
	vk::Buffer indexBuffer;
	uint32_t offset;
	uint32_t count;
};

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

	vk::DescriptorSetLayout descSetLayout() const { return mDrawParamDescLayout->layout; }

	void createPipeline(
		vk::Extent2D extent, zvk::ShaderManager* shaderManager,
		const std::vector<vk::DescriptorSetLayout>& descLayouts);

	void render(vk::CommandBuffer cmd, vk::Extent2D extent, uint32_t inFlightIdx, uint32_t curFrame, const GBufferRenderParam& param);

	void initDescriptor();

	void recreateFrame(vk::Extent2D extent);

private:
	void createDrawBuffer(const Resource& resource);
	void createResource(vk::Extent2D extent);
	void createRenderPass(vk::ImageLayout outLayout);
	void createFramebuffer(vk::Extent2D extent);
	void createDescriptor();

	void destroyFrame();

public:
	std::unique_ptr<zvk::Image> depthNormal[NumFramesInFlight][2];
	std::unique_ptr<zvk::Image> albedoMatId[NumFramesInFlight][2];
	std::unique_ptr<zvk::Image> motionVector[NumFramesInFlight];
	vk::Framebuffer framebuffer[NumFramesInFlight][2];

private:
	vk::Pipeline mPipeline;
	vk::RenderPass mRenderPass;
	vk::PipelineLayout mPipelineLayout;

	std::vector<GBufferDrawParam> mDrawParams;
	std::unique_ptr<zvk::Buffer> mDrawCommandBuffer;
	std::unique_ptr<zvk::Buffer> mDrawParamBuffer;

	std::unique_ptr<zvk::Image> mDepthStencil[NumFramesInFlight][2];

	std::unique_ptr<zvk::DescriptorPool> mDescriptorPool;
	std::unique_ptr<zvk::DescriptorSetLayout> mDrawParamDescLayout;
	vk::DescriptorSet mDrawParamDescSet;

	const bool mMultiDrawSupport;
};