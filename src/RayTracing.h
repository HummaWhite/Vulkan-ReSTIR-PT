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

struct RayTracing : public zvk::BaseVkObject {
	enum Mode : uint32_t {
		RayQuery = 0, RayPipeline = 1, Undefined
	};

	RayTracing(const zvk::Context* ctx) : BaseVkObject(ctx) {}

	void createPipeline(
		zvk::ShaderManager* shaderManager, const File::path& rayQueryShader, const File::path& rayGenShader,
		const std::vector<vk::DescriptorSetLayout>& descLayouts,
		uint32_t pushConstantSize = 0);

	void execute(vk::CommandBuffer cmd, vk::Extent2D extent, const zvk::DescriptorSetBindingMap& descSetBindings, const void* pushConstant = nullptr);
	bool GUI();

	vk::PipelineStageFlagBits pipelineStage() const {
		return (mode == RayQuery) ? vk::PipelineStageFlagBits::eComputeShader : vk::PipelineStageFlagBits::eRayTracingShaderKHR;
	}

	std::unique_ptr<zvk::ComputePipeline> rayQuery;
	std::unique_ptr<RayTracingPipelineSimple> rayPipeline;
	Mode mode = RayQuery;
};