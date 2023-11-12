#include "ComputeRayTrace.h"
#include "RayTracing.h"
#include "shader/HostDevice.h"

void ComputeRayTrace::destroy() {
	mCtx->device.destroyPipeline(mPipeline);
	mCtx->device.destroyPipelineLayout(mPipelineLayout);
}

void ComputeRayTrace::render(vk::CommandBuffer cmd, vk::Extent2D extent, const RayTracingRenderParam& param) {
	constexpr auto bindPoint = vk::PipelineBindPoint::eCompute;

	cmd.bindPipeline(bindPoint, mPipeline);

	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, CameraDescSet, param.cameraDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, ResourceDescSet, param.resourceDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, RayImageDescSet, param.rayImageDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, RayTracingDescSet, param.rayTracingDescSet, {});

	cmd.dispatch(zvk::ceilDiv(extent.width, RayQueryBlockSizeX), zvk::ceilDiv(extent.height, RayQueryBlockSizeY), 1);
}

void ComputeRayTrace::createPipeline(zvk::ShaderManager* shaderManager, const File::path& shaderPath, const std::vector<vk::DescriptorSetLayout>& descLayouts) {
	auto stageInfo = zvk::ShaderManager::shaderStageCreateInfo(
		shaderManager->createShaderModule(shaderPath),
		vk::ShaderStageFlagBits::eCompute
	);

	auto pipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo()
		.setSetLayouts(descLayouts);

	mPipelineLayout = mCtx->device.createPipelineLayout(pipelineLayoutCreateInfo);

	auto pipelineCreateInfo = vk::ComputePipelineCreateInfo()
		.setLayout(mPipelineLayout)
		.setStage(stageInfo);

	auto result = mCtx->device.createComputePipeline({}, pipelineCreateInfo);

	if (result.result != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to create Compute ray tracing pipeline with shader: " + shaderPath.generic_string());
	}
	mPipeline = result.value;
}
