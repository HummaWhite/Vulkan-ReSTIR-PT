#pragma once

#include <zvk.hpp>

class RayTracingPipelineSimple : public zvk::BaseVkObject {
public:
	RayTracingPipelineSimple(const zvk::Context* ctx) : BaseVkObject(ctx) {}
	~RayTracingPipelineSimple() { destroy(); }
	void destroy();

	void createPipeline(
		zvk::ShaderManager* shaderManager, const File::path& rayGenShaderPath,
		const std::vector<vk::DescriptorSetLayout>& descLayouts,
		uint32_t pushConstantSize = 0, uint32_t maxDepth = 2);

	void execute(
		vk::CommandBuffer cmd, vk::Extent2D extent,
		const zvk::DescriptorSetBindingMap& descSetBindings, const void* pushConstant = nullptr, uint32_t maxDepth = 1);

private:
	vk::Pipeline mPipeline;
	vk::PipelineLayout mPipelineLayout;
	std::unique_ptr<zvk::ShaderBindingTable> mShaderBindingTable;
	uint32_t mPushConstantSize = 0;
};