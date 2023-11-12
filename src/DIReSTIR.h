#pragma once

#include <zvk.hpp>

struct RayTracingRenderParam;

class DIReSTIR : public zvk::BaseVkObject {
public:
	DIReSTIR(const zvk::Context* ctx) : BaseVkObject(ctx) {}
	~DIReSTIR() { destroy(); }
	void destroy();

	void createPipeline(zvk::ShaderManager* shaderManager, const std::vector<vk::DescriptorSetLayout>& descLayouts, uint32_t maxDepth);

	void render(vk::CommandBuffer cmd, vk::Extent2D extent, const RayTracingRenderParam& param);

private:
	vk::Pipeline mRayTracingPipeline;
	vk::PipelineLayout mRayTracingPipelineLayout;
	std::unique_ptr<zvk::ShaderBindingTable> mShaderBindingTable;
};