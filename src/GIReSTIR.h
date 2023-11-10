#pragma once

#include "zvk.h"

struct RayTracingRenderParam;

class GIReSTIR : public zvk::BaseVkObject {
public:
	GIReSTIR(const zvk::Context* ctx) : BaseVkObject(ctx) {}
	~GIReSTIR() { destroy(); }
	void destroy();

	void createPipeline(zvk::ShaderManager* shaderManager, uint32_t maxDepth, const std::vector<vk::DescriptorSetLayout>& descLayouts);

	void render(vk::CommandBuffer cmd, vk::Extent2D extent, const RayTracingRenderParam& param);

private:
	vk::Pipeline mRayTracingPipeline;
	vk::PipelineLayout mRayTracingPipelineLayout;
	std::unique_ptr<zvk::ShaderBindingTable> mShaderBindingTable;
};