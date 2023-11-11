#pragma once

#include "zvk.h"

struct RayTracingRenderParam;

class RayQueryComp : public zvk::BaseVkObject {
public:
	RayQueryComp(const zvk::Context* ctx) : BaseVkObject(ctx) {}
	~RayQueryComp() { destroy(); }
	void destroy();

	void createPipeline(zvk::ShaderManager* shaderManager, const File::path& shaderPath, const std::vector<vk::DescriptorSetLayout>& descLayoutsh);

	void render(vk::CommandBuffer cmd, vk::Extent2D extent, const RayTracingRenderParam& param);

private:
	vk::Pipeline mPipeline;
	vk::PipelineLayout mPipelineLayout;
};