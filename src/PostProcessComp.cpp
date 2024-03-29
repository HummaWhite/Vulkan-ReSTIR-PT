#include "PostProcessComp.h"
#include "shader/HostDevice.h"
#include "util/Math.h"

PostProcessComp::PostProcessComp(const zvk::Context* ctx, const zvk::Swapchain* swapchain) : zvk::BaseVkObject(ctx) {
	createDescriptor(swapchain->numImages());
}

void PostProcessComp::destroy() {
	delete mDescriptorSetLayout;
	delete mDescriptorPool;

	mCtx->device.destroyPipeline(mPipeline);
	mCtx->device.destroyPipelineLayout(mPipelineLayout);
}

void PostProcessComp::render(vk::CommandBuffer cmd, vk::Extent2D extent, uint32_t imageIdx) {
	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, mPipeline);
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, mPipelineLayout, SwapchainStorageDescSet, mDescriptorSet[0][imageIdx], {});

	glm::ivec2 frameSize(extent.width, extent.height);
	PushConstant pushConstant{ frameSize, toneMapping };
	cmd.pushConstants(mPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(PushConstant), &pushConstant);

	cmd.dispatch(util::ceilDiv(extent.width, PostProcBlockSizeX), util::ceilDiv(extent.height, PostProcBlockSizeY), 1);
}

void PostProcessComp::updateDescriptor(zvk::Image* inImage[2], const zvk::Swapchain* swapchain) {
	zvk::DescriptorWrite update(mCtx);

	for (int i = 0; i < 2; i++) {
		for (uint32_t j = 0; j < swapchain->numImages(); j++) {
			update.add(
				mDescriptorSetLayout, mDescriptorSet[i][j], 0,
				vk::DescriptorImageInfo({}, swapchain->imageViews()[j], vk::ImageLayout::eGeneral)
			);
			update.add(
				mDescriptorSetLayout, mDescriptorSet[i][j], 1,
				vk::DescriptorImageInfo(inImage[i]->sampler, inImage[i]->view, inImage[i]->layout)
			);
		}
		update.flush();
	}
}

void PostProcessComp::swap() {
	std::swap(mDescriptorSet[0], mDescriptorSet[1]);
}

void PostProcessComp::createPipeline(zvk::ShaderManager* shaderManager, const std::vector<vk::DescriptorSetLayout>& descLayouts) {
	auto stageInfo = zvk::ShaderManager::shaderStageCreateInfo(
		shaderManager->createShaderModule("shaders/postProc.comp.spv"),
		vk::ShaderStageFlagBits::eCompute
	);
	vk::PushConstantRange pushConstant(vk::ShaderStageFlagBits::eCompute, 0, sizeof(PushConstant));

	auto pipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo()
		.setSetLayouts(descLayouts)
		.setPushConstantRanges(pushConstant);

	mPipelineLayout = mCtx->device.createPipelineLayout(pipelineLayoutCreateInfo);

	auto pipelineCreateInfo = vk::ComputePipelineCreateInfo()
		.setLayout(mPipelineLayout)
		.setStage(stageInfo);

	auto result = mCtx->device.createComputePipeline({}, pipelineCreateInfo);

	if (result.result != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to create PostProcessComp pipeline");
	}
	mPipeline = result.value;
}

void PostProcessComp::createDescriptor(uint32_t nSwapchainImages) {
	std::vector<vk::DescriptorSetLayoutBinding> bindings = {
		zvk::Descriptor::makeBinding(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute),
		zvk::Descriptor::makeBinding(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute)
	};
	mDescriptorSetLayout = new zvk::DescriptorSetLayout(mCtx, bindings);

	mDescriptorPool = new zvk::DescriptorPool(mCtx, { mDescriptorSetLayout }, 2 * nSwapchainImages);
	mDescriptorSet[0] = mDescriptorPool->allocDescriptorSets(mDescriptorSetLayout->layout, nSwapchainImages);
	mDescriptorSet[1] = mDescriptorPool->allocDescriptorSets(mDescriptorSetLayout->layout, nSwapchainImages);
}
