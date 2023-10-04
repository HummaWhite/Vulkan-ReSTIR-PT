#pragma once

#include "RayTracingCommon.h"

class ResampledDIPass : public zvk::BaseVkObject {
public:
	ResampledDIPass(const zvk::Context* ctx) : BaseVkObject(ctx) {}
	~ResampledDIPass() { destroy(); }
	void destroy();

	void createPipeline(zvk::ShaderManager* shaderManager, uint32_t maxDepth, const std::vector<vk::DescriptorSetLayout>& descLayouts);

	void render(vk::CommandBuffer cmd, vk::Extent2D extent, const RayTracingRenderParam& param);

private:
	vk::Pipeline mPipeline;
	vk::PipelineLayout mPipelineLayout;
	std::unique_ptr<zvk::ShaderBindingTable> mShaderBindingTable;
};