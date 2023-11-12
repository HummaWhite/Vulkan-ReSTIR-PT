#pragma once

#include <zvk.hpp>

struct RayTracingRenderParam;

class ComputeRayTrace : public zvk::BaseVkObject {
public:
	ComputeRayTrace(const zvk::Context* ctx) : BaseVkObject(ctx) {}
	~ComputeRayTrace() { destroy(); }
	void destroy();

	void createPipeline(zvk::ShaderManager* shaderManager, const File::path& shaderPath, const std::vector<vk::DescriptorSetLayout>& descLayoutsh);

	void render(vk::CommandBuffer cmd, vk::Extent2D extent, const RayTracingRenderParam& param);

private:
	vk::Pipeline mPipeline;
	vk::PipelineLayout mPipelineLayout;
};