#pragma once

#include "zvk.h"
#include "Scene.h"

struct RayTracingRenderParam;

class ResampledPTPass : public zvk::BaseVkObject {
public:
	ResampledPTPass(const zvk::Context* ctx) : BaseVkObject(ctx) {}
	~ResampledPTPass() { destroy(); }
	void destroy();

	void createPipeline(zvk::ShaderManager* shaderManager, uint32_t maxDepth, const std::vector<vk::DescriptorSetLayout>& descLayouts);

	void render(vk::CommandBuffer cmd, vk::Extent2D extent, const RayTracingRenderParam& param);

private:
	vk::Pipeline mRayTracingPipeline;
	vk::PipelineLayout mRayTracingPipelineLayout;
	std::unique_ptr<zvk::ShaderBindingTable> mShaderBindingTable;
};