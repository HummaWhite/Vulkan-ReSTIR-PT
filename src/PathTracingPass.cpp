#include "PathTracingPass.h"
#include "shader/HostDevice.h"
#include "core/ExtFunctions.h"
#include "core/DebugUtils.h"

PathTracingPass::PathTracingPass(const zvk::Context* ctx, const Resource& resource, const DeviceScene* scene, vk::Extent2D extent, zvk::QueueIdx queueIdx) :
	zvk::BaseVkObject(ctx)
{
	createAccelerationStructure(resource, scene, queueIdx);
	createFrame(extent, queueIdx);
	createDescriptor();
}

void PathTracingPass::destroy() {
	destroyFrame();

	/*
	delete mShaderBindingTable;
	delete mTLAS;

	for (auto& blas : mObjectBLAS) {
		delete blas;
	}
	
	delete mDescriptorSetLayout;
	delete mDescriptorPool;
	*/

	mCtx->device.destroyPipeline(mPipeline);
	mCtx->device.destroyPipelineLayout(mPipelineLayout);
}

void PathTracingPass::render(
	vk::CommandBuffer cmd, uint32_t frameIdx,
	vk::DescriptorSet cameraDescSet, vk::DescriptorSet resourceDescSet, vk::DescriptorSet imageOutDescSet,
	vk::Extent2D extent, uint32_t maxDepth
) {
	auto bindPoint = vk::PipelineBindPoint::eRayTracingKHR;

	cmd.bindPipeline(bindPoint, mPipeline);

	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, CameraDescSet, cameraDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, ResourceDescSet, resourceDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, ImageOutputDescSet, imageOutDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, RayTracingDescSet, mDescriptorSet, {});

	zvk::ExtFunctions::cmdTraceRaysKHR(cmd, mRayGenRegion, mMissRegion, mHitRegion, mCallRegion, extent.width, extent.height, maxDepth);
}

void PathTracingPass::initDescriptor() {
	zvk::DescriptorWrite update;

	update.add(
		mDescriptorSetLayout.get(), mDescriptorSet, 0,
		vk::WriteDescriptorSetAccelerationStructureKHR(mTLAS->structure)
	);
	mCtx->device.updateDescriptorSets(update.writes, {});
}

void PathTracingPass::recreateFrame(vk::Extent2D extent, zvk::QueueIdx queueIdx) {
	destroyFrame();
	createFrame(extent, queueIdx);
}

void PathTracingPass::createPipeline(zvk::ShaderManager* shaderManager, uint32_t maxDepth, const std::vector<vk::DescriptorSetLayout>& descLayouts) {
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

	createShaderBindingTable();
}

void PathTracingPass::createAccelerationStructure(const Resource& resource, const DeviceScene* scene, zvk::QueueIdx queueIdx) {
	for (auto model : resource.uniqueModelInstances()) {
		auto firstMesh = resource.meshInstances()[model->meshOffset()];

		uint64_t indexOffset = firstMesh.indexOffset * sizeof(uint32_t);

		zvk::AccelerationStructureTriangleMesh meshData {
			.vertexAddress = scene->vertices->address(),
			.indexAddress = scene->indices->address() + indexOffset,
			.vertexStride = sizeof(MeshVertex),
			.vertexFormat = vk::Format::eR32G32B32Sfloat,
			.indexType = vk::IndexType::eUint32,
			.maxVertex = model->numVertices(),
			.numIndices = model->numIndices(),
			.indexOffset = firstMesh.indexOffset
		};

		auto BLAS = std::make_unique<zvk::AccelerationStructure>(
			mCtx, zvk::QueueIdx::GeneralUse, meshData, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
		);
		zvk::DebugUtils::nameVkObject(mCtx->device, BLAS->structure, "BLAS_" + model->name());

		mObjectBLAS.push_back(std::move(BLAS));
	}
	std::vector<vk::AccelerationStructureInstanceKHR> instances;

	for (uint32_t i = 0; i < resource.modelInstances().size(); i++) {
		auto modelInstance = resource.modelInstances()[i];

		vk::TransformMatrixKHR transform;
		glm::mat4 matrix = glm::transpose(modelInstance->modelMatrix());
		memcpy(&transform, &matrix, 12 * sizeof(float));

		instances.push_back(
			vk::AccelerationStructureInstanceKHR()
				.setTransform(transform)
				.setInstanceCustomIndex(i)
				.setMask(0xff)
				.setInstanceShaderBindingTableRecordOffset(0)
				.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable)
				.setAccelerationStructureReference(mObjectBLAS[modelInstance->refId()]->address)
		);
	}

	mTLAS = std::make_unique<zvk::AccelerationStructure>(
		mCtx, zvk::QueueIdx::GeneralUse, instances, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, mTLAS->structure, "TLAS");
}

void PathTracingPass::createFrame(vk::Extent2D extent, zvk::QueueIdx queueIdx) {
	auto cmd = zvk::Command::createOneTimeSubmit(mCtx, queueIdx);

	for (int i = 0; i < 2; i++) {
		colorOutput[i] = zvk::Memory::createImage2D(
			mCtx, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		colorOutput[i]->createImageView();
		colorOutput[i]->createSampler(vk::Filter::eLinear);

		zvk::DebugUtils::nameVkObject(mCtx->device, colorOutput[i]->image, "colorOutput[" + std::to_string(i) + "]");

		auto barrier = colorOutput[i]->getBarrier(
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

void PathTracingPass::createShaderBindingTable() {
	// TODO:
	// write a wrapper class for Shader Binding Table

	const auto& pipelineProps = mCtx->instance()->rayTracingPipelineProperties;

	uint32_t numMissGroup = 2;
	uint32_t numHitGroup = 1;
	uint32_t numGroups = 1 + numMissGroup + numHitGroup;
	uint32_t handleSize = pipelineProps.shaderGroupHandleSize;

	uint32_t alignedHandleSize = zvk::align(pipelineProps.shaderGroupHandleSize, pipelineProps.shaderGroupHandleAlignment);
	uint32_t baseAlignment = pipelineProps.shaderGroupBaseAlignment;

	mRayGenRegion.stride = zvk::align(alignedHandleSize, baseAlignment);
	mRayGenRegion.size = mRayGenRegion.stride;

	mMissRegion.stride = alignedHandleSize;
	mMissRegion.size = zvk::align(numMissGroup * alignedHandleSize, baseAlignment);

	mHitRegion.stride = alignedHandleSize;
	mHitRegion.size = zvk::align(numHitGroup * alignedHandleSize, baseAlignment);

	uint32_t dataSize = numGroups * handleSize;
	auto handles = zvk::ExtFunctions::getRayTracingShaderGroupHandlesKHR(mCtx->device, mPipeline, 0, numGroups, dataSize);

	vk::DeviceSize SBTSize = mRayGenRegion.size + mMissRegion.size + mHitRegion.size + mCallRegion.size;

	mShaderBindingTable = zvk::Memory::createBuffer(
		mCtx, SBTSize,
		vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress |
			vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		vk::MemoryAllocateFlagBits::eDeviceAddress
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, mShaderBindingTable->buffer, "shader binding table");

	mShaderBindingTable->mapMemory();

	vk::DeviceAddress SBTAddress = mShaderBindingTable->address();
	mRayGenRegion.deviceAddress = SBTAddress;
	mMissRegion.deviceAddress = SBTAddress + mRayGenRegion.size;
	mHitRegion.deviceAddress = SBTAddress + mRayGenRegion.size + mMissRegion.size;

	auto pDst = reinterpret_cast<uint8_t*>(mShaderBindingTable->data);
	auto pSrc = reinterpret_cast<uint8_t*>(handles.data());

	memcpy(pDst, pSrc, handleSize);
	
	pDst += mRayGenRegion.size;
	pSrc += handleSize;

	for (uint32_t i = 0; i < numMissGroup; i++) {
		memcpy(pDst + i * alignedHandleSize, pSrc + i * handleSize, handleSize);
	}
	pDst += mMissRegion.size;
	pSrc += numMissGroup * handleSize;

	for (uint32_t i = 0; i < numHitGroup; i++) {
		memcpy(pDst + i * alignedHandleSize, pSrc + i * handleSize, handleSize);
	}
	mShaderBindingTable->unmapMemory();
}

void PathTracingPass::createDescriptor() {
	std::vector<vk::DescriptorSetLayoutBinding> bindings = {
		zvk::Descriptor::makeBinding(0, vk::DescriptorType::eAccelerationStructureKHR, RayTracingShaderStageFlags)
	};

	mDescriptorSetLayout = std::make_unique<zvk::DescriptorSetLayout>(mCtx, bindings);
	mDescriptorPool = std::make_unique<zvk::DescriptorPool>(mCtx, mDescriptorSetLayout.get(), 1);
	mDescriptorSet = mDescriptorPool->allocDescriptorSet(mDescriptorSetLayout->layout);
}

void PathTracingPass::destroyFrame() {
}
