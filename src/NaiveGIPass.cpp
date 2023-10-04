#include "NaiveGIPass.h"
#include "shader/HostDevice.h"
#include "core/ExtFunctions.h"
#include "core/DebugUtils.h"

void NaiveGIPass::destroy() {
	mCtx->device.destroyPipeline(mPipeline);
	mCtx->device.destroyPipelineLayout(mPipelineLayout);
}

void NaiveGIPass::render(vk::CommandBuffer cmd, vk::Extent2D extent, const RayTracingRenderParam& param) {
	auto bindPoint = vk::PipelineBindPoint::eRayTracingKHR;

	cmd.bindPipeline(bindPoint, mPipeline);

	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, CameraDescSet, param.cameraDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, ResourceDescSet, param.resourceDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, RayImageDescSet, param.rayImageDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, RayTracingDescSet, param.rayTracingDescSet, {});

	zvk::ExtFunctions::cmdTraceRaysKHR(
		cmd,
		mShaderBindingTable->rayGenRegion, mShaderBindingTable->missRegion, mShaderBindingTable->hitRegion, mShaderBindingTable->callableRegion,
		extent.width, extent.height, param.maxDepth
	);
}

void NaiveGIPass::createPipeline(zvk::ShaderManager* shaderManager, uint32_t maxDepth, const std::vector<vk::DescriptorSetLayout>& descLayouts) {
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
		shaderManager->createShaderModule("shaders/naiveGIPass.rgen.spv"), vk::ShaderStageFlagBits::eRaygenKHR
	);
	stages[Miss] = zvk::ShaderManager::shaderStageCreateInfo(
		shaderManager->createShaderModule("shaders/rayTracingMiss.rmiss.spv"), vk::ShaderStageFlagBits::eMissKHR
	);
	stages[ShadowMiss] = zvk::ShaderManager::shaderStageCreateInfo(
		shaderManager->createShaderModule("shaders/rayTracingShadow.rmiss.spv"), vk::ShaderStageFlagBits::eMissKHR
	);
	stages[ClosestHit] = zvk::ShaderManager::shaderStageCreateInfo(
		shaderManager->createShaderModule("shaders/intersection.rchit.spv"), vk::ShaderStageFlagBits::eClosestHitKHR
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