#pragma once

#include "zvk.h"

struct RayTracingRenderParam;

class NaivePathTrace : public zvk::BaseVkObject {
public:
	NaivePathTrace(const zvk::Context* ctx) : BaseVkObject(ctx) {}
	~NaivePathTrace() { destroy(); }
	void destroy();

	void createPipeline(
		zvk::ShaderManager* shaderManager, const File::path& rayGenShaderPath,
		const std::vector<vk::DescriptorSetLayout>& descLayouts,
		uint32_t maxDepth);

	void render(vk::CommandBuffer cmd, vk::Extent2D extent, const RayTracingRenderParam& param);

private:
	vk::Pipeline mRayTracingPipeline;
	vk::PipelineLayout mRayTracingPipelineLayout;
	std::unique_ptr<zvk::ShaderBindingTable> mShaderBindingTable;
};