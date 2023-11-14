#pragma once

#include <map>

#include "Context.h"
#include "ShaderManager.h"
#include "Descriptor.h"

NAMESPACE_BEGIN(zvk)

class ComputePipeline : public zvk::BaseVkObject {
public:
	ComputePipeline(const zvk::Context* ctx) : BaseVkObject(ctx) {}
	~ComputePipeline() { destroy(); }
	void destroy();

	void createPipeline(
		zvk::ShaderManager* shaderManager, const File::path& shaderPath,
		const std::vector<vk::DescriptorSetLayout>& descLayouts,
		uint32_t pushConstantSize = 0);

	void execute(
		vk::CommandBuffer cmd, vk::Extent3D launchSize, vk::Extent3D blockSize,
		const DescriptorSetBindingMap& descSetBindings, const void* pushConstant = nullptr);

private:
	vk::Pipeline mPipeline;
	vk::PipelineLayout mPipelineLayout;
	uint32_t mPushConstantSize = 0;
};

NAMESPACE_END(zvk)