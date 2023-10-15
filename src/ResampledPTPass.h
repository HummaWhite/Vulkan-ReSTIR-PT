#pragma once

#include "RayTracingCommon.h"

class ResampledPTPass : public zvk::BaseVkObject {
public:
	ResampledPTPass(const zvk::Context* ctx) : BaseVkObject(ctx) {}
	~ResampledPTPass() { destroy(); }
	void destroy();

	void createPipeline(zvk::ShaderManager* shaderManager, uint32_t maxDepth, const std::vector<vk::DescriptorSetLayout>& descLayouts);

	void render(vk::CommandBuffer cmd, vk::Extent2D extent, const RayTracingRenderParam& param);

private:
	vk::Pipeline mPipeline;
	vk::PipelineLayout mPipelineLayout;
	std::unique_ptr<zvk::ShaderBindingTable> mShaderBindingTable;
};