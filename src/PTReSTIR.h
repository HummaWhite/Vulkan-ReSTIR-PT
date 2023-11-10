#pragma once

#include "zvk.h"

struct RayTracingRenderParam;

class PTReSTIR : public zvk::BaseVkObject {
public:
	PTReSTIR(const zvk::Context* ctx) : BaseVkObject(ctx) {}
	~PTReSTIR() { destroy(); }
	void destroy();

	void createPipeline(zvk::ShaderManager* shaderManager, uint32_t maxDepth, const std::vector<vk::DescriptorSetLayout>& descLayouts);

	void render(vk::CommandBuffer cmd, vk::Extent2D extent, const RayTracingRenderParam& param);

private:
	vk::Pipeline mRayTracingPipeline;
	vk::PipelineLayout mRayTracingPipelineLayout;
	std::unique_ptr<zvk::ShaderBindingTable> mShaderBindingTable;
};