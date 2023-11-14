#include "ComputePipeline.h"
#include "util/Math.h"

NAMESPACE_BEGIN(zvk)

void ComputePipeline::destroy() {
	mCtx->device.destroyPipeline(mPipeline);
	mCtx->device.destroyPipelineLayout(mPipelineLayout);
}

void ComputePipeline::execute(
	vk::CommandBuffer cmd, vk::Extent3D launchSize, vk::Extent3D blockSize,
	const DescriptorSetBindingMap& descSetBindings, const void* pushConstant
) {
	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, mPipeline);

	for (const auto& binding : descSetBindings) {
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, mPipelineLayout, binding.first, binding.second, {});
	}

	if (pushConstant) {
		cmd.pushConstants(mPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, mPushConstantSize, pushConstant);
	}
	cmd.dispatch(util::ceilDiv(launchSize.width, blockSize.width), util::ceilDiv(launchSize.height, blockSize.height), util::ceilDiv(launchSize.depth, blockSize.depth));
}

void ComputePipeline::createPipeline(
	zvk::ShaderManager* shaderManager, const File::path& shaderPath,
	const std::vector<vk::DescriptorSetLayout>& descLayouts,
	uint32_t pushConstantSize
) {
	auto stageInfo = zvk::ShaderManager::shaderStageCreateInfo(
		shaderManager->createShaderModule(shaderPath),
		vk::ShaderStageFlagBits::eCompute
	);

	auto pushConstantRange = vk::PushConstantRange()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setOffset(0)
		.setSize(pushConstantSize);

	auto pipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo()
		.setSetLayouts(descLayouts);

	if (pushConstantSize > 0) {
		pipelineLayoutCreateInfo.setPushConstantRanges(pushConstantRange);
		mPushConstantSize = pushConstantSize;
	}

	mPipelineLayout = mCtx->device.createPipelineLayout(pipelineLayoutCreateInfo);

	auto pipelineCreateInfo = vk::ComputePipelineCreateInfo()
		.setLayout(mPipelineLayout)
		.setStage(stageInfo);

	auto result = mCtx->device.createComputePipeline({}, pipelineCreateInfo);

	if (result.result != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to create {compute pipeline} with shader: " + shaderPath.generic_string());
	}
	mPipeline = result.value;
}

NAMESPACE_END(zvk);
