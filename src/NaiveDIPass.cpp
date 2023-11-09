#include "NaiveDIPass.h"
#include "shader/HostDevice.h"
#include "core/ExtFunctions.h"
#include "core/DebugUtils.h"
#include "RayTracing.h"

void NaiveDIPass::destroy() {
	mCtx->device.destroyPipeline(mRayTracingPipeline);
	mCtx->device.destroyPipelineLayout(mRayTracingPipelineLayout);
}

void NaiveDIPass::render(vk::CommandBuffer cmd, vk::Extent2D extent, const RayTracingRenderParam& param) {
	auto bindPoint = vk::PipelineBindPoint::eRayTracingKHR;

	cmd.bindPipeline(bindPoint, mRayTracingPipeline);

	cmd.bindDescriptorSets(bindPoint, mRayTracingPipelineLayout, CameraDescSet, param.cameraDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mRayTracingPipelineLayout, ResourceDescSet, param.resourceDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mRayTracingPipelineLayout, RayImageDescSet, param.rayImageDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mRayTracingPipelineLayout, RayTracingDescSet, param.rayTracingDescSet, {});

	zvk::ExtFunctions::cmdTraceRaysKHR(
		cmd,
		mShaderBindingTable->rayGenRegion, mShaderBindingTable->missRegion, mShaderBindingTable->hitRegion, mShaderBindingTable->callableRegion,
		extent.width, extent.height, param.maxDepth
	);
}

void NaiveDIPass::createPipeline(zvk::ShaderManager* shaderManager, uint32_t maxDepth, const std::vector<vk::DescriptorSetLayout>& descLayouts) {
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
		shaderManager->createShaderModule("shaders/di_naive.rgen.spv"), vk::ShaderStageFlagBits::eRaygenKHR
	);
	stages[Miss] = zvk::ShaderManager::shaderStageCreateInfo(
		shaderManager->createShaderModule("shaders/ray_miss.rmiss.spv"), vk::ShaderStageFlagBits::eMissKHR
	);
	stages[ShadowMiss] = zvk::ShaderManager::shaderStageCreateInfo(
		shaderManager->createShaderModule("shaders/ray_shadow.rmiss.spv"), vk::ShaderStageFlagBits::eMissKHR
	);
	stages[ClosestHit] = zvk::ShaderManager::shaderStageCreateInfo(
		shaderManager->createShaderModule("shaders/ray_intersection.rchit.spv"), vk::ShaderStageFlagBits::eClosestHitKHR
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

	mRayTracingPipelineLayout = mCtx->device.createPipelineLayout(pipelineLayoutCreateInfo);

	auto pipelineCreateInfo = vk::RayTracingPipelineCreateInfoKHR()
		.setLayout(mRayTracingPipelineLayout)
		.setStages(stages)
		.setGroups(groups)
		.setMaxPipelineRayRecursionDepth(maxDepth);

	auto result = zvk::ExtFunctions::createRayTracingPipelineKHR(mCtx->device, {}, {}, pipelineCreateInfo);

	if (result.result != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to create RayTracingPass pipeline");
	}
	mRayTracingPipeline = result.value;

	mShaderBindingTable = std::make_unique<zvk::ShaderBindingTable>(mCtx, 2, 1, mRayTracingPipeline);
}
