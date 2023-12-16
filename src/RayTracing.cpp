#include "RayTracing.h"
#include "shader/HostDevice.h"
#include "util/Error.h"

#include <imgui.h>

void RayTracingPipelineSimple::destroy() {
	mCtx->device.destroyPipeline(mPipeline);
	mCtx->device.destroyPipelineLayout(mPipelineLayout);
}

void RayTracingPipelineSimple::execute(
	vk::CommandBuffer cmd, vk::Extent2D extent,
	const zvk::DescriptorSetBindingMap& descSetBindings, const void* pushConstant, uint32_t maxDepth
) {
	cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, mPipeline);

	for (const auto& binding : descSetBindings) {
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, mPipelineLayout, binding.first, binding.second, {});
	}

	if (pushConstant) {
		cmd.pushConstants(mPipelineLayout, vk::ShaderStageFlagBits::eRaygenKHR, 0, mPushConstantSize, pushConstant);
	}

	zvk::ExtFunctions::cmdTraceRaysKHR(
		cmd,
		mShaderBindingTable->rayGenRegion, mShaderBindingTable->missRegion, mShaderBindingTable->hitRegion, mShaderBindingTable->callableRegion,
		extent.width, extent.height, maxDepth
	);
}

void RayTracingPipelineSimple::createPipeline(
	zvk::ShaderManager* shaderManager, const File::path& rayGenShaderPath,
	const std::vector<vk::DescriptorSetLayout>& descLayouts,
	uint32_t pushConstantSize, uint32_t maxDepth
) {
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
		shaderManager->createShaderModule(rayGenShaderPath), vk::ShaderStageFlagBits::eRaygenKHR
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

	auto pushConstantRange = vk::PushConstantRange()
		.setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR)
		.setOffset(0)
		.setSize(pushConstantSize);

	auto pipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo()
		.setSetLayouts(descLayouts);

	if (pushConstantSize > 0) {
		pipelineLayoutCreateInfo.setPushConstantRanges(pushConstantRange);
		mPushConstantSize = pushConstantSize;
	}

	mPipelineLayout = mCtx->device.createPipelineLayout(pipelineLayoutCreateInfo);

	auto pipelineCreateInfo = vk::RayTracingPipelineCreateInfoKHR()
		.setLayout(mPipelineLayout)
		.setStages(stages)
		.setGroups(groups)
		.setMaxPipelineRayRecursionDepth(maxDepth);

	auto result = zvk::ExtFunctions::createRayTracingPipelineKHR(mCtx->device, {}, {}, pipelineCreateInfo);

	if (result.result != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to create RayTracingPipelineSimple");
	}

	mPipeline = result.value;
	mShaderBindingTable = std::make_unique<zvk::ShaderBindingTable>(mCtx, 2, 1, mPipeline);
}

void RayTracing::createPipeline(
	zvk::ShaderManager* shaderManager, const File::path& rayQueryShader, const File::path& rayGenShader,
	const std::vector<vk::DescriptorSetLayout>& descLayouts, uint32_t pushConstantSize
) {
	if (!rayQueryShader.empty()) {
		rayQuery = std::make_unique<zvk::ComputePipeline>(mCtx);
		rayQuery->createPipeline(shaderManager, rayQueryShader, descLayouts, pushConstantSize);
	}

	if (!rayGenShader.empty()) {
		rayPipeline = std::make_unique<RayTracingPipelineSimple>(mCtx);
		rayPipeline->createPipeline(shaderManager, rayGenShader, descLayouts, pushConstantSize);
	}

	if (!rayQuery && !rayPipeline) {
		mode = Undefined;
	}
}

void RayTracing::execute(vk::CommandBuffer cmd, vk::Extent2D extent, const zvk::DescriptorSetBindingMap& descSetBindings, const void* pushConstant) {
	if (mode == RayQuery && rayQuery) {
		rayQuery->execute(cmd, vk::Extent3D(extent, 1), vk::Extent3D(RayQueryBlockSizeX, RayQueryBlockSizeY, 1), descSetBindings, pushConstant);
	}
	else if (mode == RayPipeline && rayPipeline) {
		rayPipeline->execute(cmd, extent, descSetBindings, pushConstant);
	}
	else {
		Log::line("RayTracing::execute: pipeline not created");
	}
}

bool RayTracing::GUI() {
	static const char* modes[] = { "Ray Query", "Ray Pipeline" };
	return ImGui::Combo("Mode", reinterpret_cast<int*>(&mode), modes, IM_ARRAYSIZE(modes));
}