#include "NaiveDirectIllumination.h"
#include "shader/HostDevice.h"
#include "core/ExtFunctions.h"
#include "core/DebugUtils.h"

NaiveDirectIllumination::NaiveDirectIllumination(const zvk::Context* ctx, const Resource& resource, const DeviceScene* scene, vk::Extent2D extent, zvk::QueueIdx queueIdx) :
	zvk::BaseVkObject(ctx)
{
	createFrame(extent, queueIdx);
}

void NaiveDirectIllumination::destroy() {
	destroyFrame();

	mCtx->device.destroyPipeline(mPipeline);
	mCtx->device.destroyPipelineLayout(mPipelineLayout);
}

void NaiveDirectIllumination::render(
	vk::CommandBuffer cmd, uint32_t frameIdx,
	vk::DescriptorSet cameraDescSet, vk::DescriptorSet resourceDescSet, vk::DescriptorSet imageOutDescSet, vk::DescriptorSet rayTracingDescSet,
	vk::Extent2D extent, uint32_t maxDepth
) {
	auto bindPoint = vk::PipelineBindPoint::eRayTracingKHR;

	cmd.bindPipeline(bindPoint, mPipeline);

	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, CameraDescSet, cameraDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, ResourceDescSet, resourceDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, ImageOutputDescSet, imageOutDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, RayTracingDescSet, rayTracingDescSet, {});

	zvk::ExtFunctions::cmdTraceRaysKHR(
		cmd,
		mShaderBindingTable->rayGenRegion, mShaderBindingTable->missRegion, mShaderBindingTable->hitRegion, mShaderBindingTable->callableRegion,
		extent.width, extent.height, maxDepth
	);
}

void NaiveDirectIllumination::recreateFrame(vk::Extent2D extent, zvk::QueueIdx queueIdx) {
	destroyFrame();
	createFrame(extent, queueIdx);
}

void NaiveDirectIllumination::createPipeline(zvk::ShaderManager* shaderManager, uint32_t maxDepth, const std::vector<vk::DescriptorSetLayout>& descLayouts) {
	enum Stages : uint32_t {
		RayGen,
		Miss,
		ShadowMiss,
		ClosestHit,
		NumStages
	};

	std::array<vk::PipelineShaderStageCreateInfo, NumStages> stages;
	std::vector<vk::RayTracingShaderGroupCreateInfoKHR> groups;

	stages[RayGen] = zvk::ShaderManager::shaderStageCreateInfo(
		shaderManager->createShaderModule("shaders/rayTrace.rgen.spv"), vk::ShaderStageFlagBits::eRaygenKHR
	);
	stages[Miss] = zvk::ShaderManager::shaderStageCreateInfo(
		shaderManager->createShaderModule("shaders/rayTrace.rmiss.spv"), vk::ShaderStageFlagBits::eMissKHR
	);
	stages[ShadowMiss] = zvk::ShaderManager::shaderStageCreateInfo(
		shaderManager->createShaderModule("shaders/rayTraceShadow.rmiss.spv"), vk::ShaderStageFlagBits::eMissKHR
	);
	stages[ClosestHit] = zvk::ShaderManager::shaderStageCreateInfo(
		shaderManager->createShaderModule("shaders/rayTrace.rchit.spv"), vk::ShaderStageFlagBits::eClosestHitKHR
	);

	groups.push_back(
		vk::RayTracingShaderGroupCreateInfoKHR(
			vk::RayTracingShaderGroupTypeKHR::eGeneral,
			RayGen, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR
		)
	);
	groups.push_back(
		vk::RayTracingShaderGroupCreateInfoKHR(
			vk::RayTracingShaderGroupTypeKHR::eGeneral,
			Miss, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR
		)
	);
	groups.push_back(
		vk::RayTracingShaderGroupCreateInfoKHR(
			vk::RayTracingShaderGroupTypeKHR::eGeneral,
			ShadowMiss, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR
		)
	);
	groups.push_back(
		vk::RayTracingShaderGroupCreateInfoKHR(
			vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
			VK_SHADER_UNUSED_KHR, ClosestHit, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR
		)
	);

	auto pipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo()
		.setSetLayouts(descLayouts);

	mPipelineLayout = mCtx->device.createPipelineLayout(pipelineLayoutCreateInfo);

	auto pipelineCreateInfo = vk::RayTracingPipelineCreateInfoKHR()
		.setLayout(mPipelineLayout)
		.setStages(stages)
		.setGroups(groups)
		.setMaxPipelineRayRecursionDepth(maxDepth);

	auto result = zvk::ExtFunctions::createRayTracingPipelineKHR(mCtx->device, {}, {}, pipelineCreateInfo);

	if (result.result != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to create RayTracingPass pipeline");
	}
	mPipeline = result.value;

	mShaderBindingTable = std::make_unique<zvk::ShaderBindingTable>(mCtx, 2, 1, mPipeline);
}

void NaiveDirectIllumination::createFrame(vk::Extent2D extent, zvk::QueueIdx queueIdx) {
	auto cmd = zvk::Command::createOneTimeSubmit(mCtx, queueIdx);

	for (int i = 0; i < 2; i++) {
		directOutput[i] = zvk::Memory::createImage2D(
			mCtx, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		directOutput[i]->createImageView();
		directOutput[i]->createSampler(vk::Filter::eLinear);

		zvk::DebugUtils::nameVkObject(mCtx->device, directOutput[i]->image, "colorOutput[" + std::to_string(i) + "]");

		auto barrier = directOutput[i]->getBarrier(
			vk::ImageLayout::eGeneral,
			vk::AccessFlagBits::eNone, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite
		);

		cmd->cmd.pipelineBarrier(
			vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eRayTracingShaderKHR,
			vk::DependencyFlags{ 0 }, {}, {}, barrier
		);

		reservoir[i] = zvk::Memory::createBuffer(
			mCtx, sizeof(Reservoir) * extent.width * extent.height, vk::BufferUsageFlagBits::eStorageBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		zvk::DebugUtils::nameVkObject(mCtx->device, reservoir[i]->buffer, "reservoir[" + std::to_string(i) + "]");
	}
	cmd->submitAndWait();
}

void NaiveDirectIllumination::destroyFrame() {
}
