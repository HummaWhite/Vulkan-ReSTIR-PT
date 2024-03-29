#pragma once

#include <zvk.hpp>

class GIReSTIR : public zvk::BaseVkObject {
public:
	GIReSTIR(const zvk::Context* ctx) : BaseVkObject(ctx) {}
	~GIReSTIR() { destroy(); }
	void destroy();

	void createPipeline(zvk::ShaderManager* shaderManager, const std::vector<vk::DescriptorSetLayout>& descLayouts, uint32_t maxDepth);

	void render(vk::CommandBuffer cmd, vk::Extent2D extent, const zvk::DescriptorSetBindingMap& descSetBindings);

private:
	vk::Pipeline mPipeline;
	vk::PipelineLayout mPipelineLayout;
	std::unique_ptr<zvk::ShaderBindingTable> mShaderBindingTable;
};