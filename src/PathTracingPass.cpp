#include "PathTracingPass.h"
#include "shader/HostDevice.h"

PathTracingPass::PathTracingPass(const zvk::Context* ctx, const Resource& resource, const DeviceScene* scene, vk::Extent2D extent, zvk::QueueIdx queueIdx) :
	zvk::BaseVkObject(ctx)
{
	createAccelerationStructure(resource, scene, queueIdx);
	createFrame(extent, queueIdx);
	createDescriptor();
}

void PathTracingPass::destroy() {
	destroyFrame();

	delete mShaderBindingTable;
	delete mTLAS;

	for (auto& blas : mObjectBLAS) {
		delete blas;
	}
	delete mDescriptorSetLayout;
	delete mDescriptorPool;

	mCtx->device.destroyPipeline(mPipeline);
	mCtx->device.destroyPipelineLayout(mPipelineLayout);
}

void PathTracingPass::render(
	vk::CommandBuffer cmd,
	vk::DescriptorSet cameraDescSet, vk::DescriptorSet resourceDescSet, vk::DescriptorSet imageOutDescSet,
	vk::Extent2D extent, uint32_t maxDepth
) {
	const auto& ext = mCtx->instance()->extFunctions();
	auto bindPoint = vk::PipelineBindPoint::eRayTracingKHR;

	cmd.bindPipeline(bindPoint, mPipeline);

	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, CameraDescSet, cameraDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, ResourceDescSet, resourceDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, ImageOutputDescSet, imageOutDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, RayTracingDescSet, mDescriptorSet, {});

	ext.cmdTraceRaysKHR(cmd, mRayGenRegion, mMissRegion, mHitRegion, mCallRegion, extent.width, extent.height, maxDepth);
}

void PathTracingPass::initDescriptor() {
	zvk::DescriptorWrite update;

	update.add(
		mDescriptorSetLayout, mDescriptorSet, 0,
		vk::WriteDescriptorSetAccelerationStructureKHR(mTLAS->structure)
	);
	mCtx->device.updateDescriptorSets(update.writes, {});
}

void PathTracingPass::swap() {
	std::swap(colorOutput[0], colorOutput[1]);
	std::swap(reservoir[0], reservoir[1]);
}

void PathTracingPass::recreateFrame(vk::Extent2D extent, zvk::QueueIdx queueIdx) {
	destroyFrame();
	createFrame(extent, queueIdx);
}

void PathTracingPass::createPipeline(zvk::ShaderManager* shaderManager, uint32_t maxDepth, const std::vector<vk::DescriptorSetLayout>& descLayouts) {
	enum Stages : uint32_t {
		RayGen,
		Miss,
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

	auto result = mCtx->instance()->extFunctions().createRayTracingPipelineKHR(mCtx->device, {}, {}, pipelineCreateInfo);

	if (result.result != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to create RayTracingPass pipeline");
	}
	mPipeline = result.value;

	createShaderBindingTable();
}

void PathTracingPass::createAccelerationStructure(const Resource& resource, const DeviceScene* scene, zvk::QueueIdx queueIdx) {
	for (const auto& model : resource.modelInstances()) {
		auto firstMesh = resource.meshInstances()[model->meshOffset()];
		uint64_t vertexOffset = firstMesh.vertexOffset * sizeof(MeshVertex);
		uint64_t indexOffset = firstMesh.indexOffset * sizeof(uint32_t);

		zvk::AccelerationStructureTriangleMesh meshData {
			.vertexAddress = scene->vertices->address(),
			.indexAddress = scene->indices->address(),
			.vertexStride = sizeof(MeshVertex),
			.vertexFormat = vk::Format::eR32G32B32Sfloat,
			.indexType = vk::IndexType::eUint32,
			.maxVertex = scene->numVertices,
			.numIndices = model->numIndices(),
			.indexOffset = resource.meshInstances()[model->meshOffset()].indexOffset
		};

		auto blas = new zvk::AccelerationStructure(
			mCtx, zvk::QueueIdx::GeneralUse, meshData, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
		);
		mObjectBLAS.push_back(blas);
	}

	std::vector<vk::AccelerationStructureInstanceKHR> instances;

	for (uint32_t i = 0; i < resource.modelInstances().size(); i++) {
		vk::TransformMatrixKHR transform;
		auto& m = transform.matrix;

		m[0][0] = 1.f, m[0][1] = 0.f, m[0][2] = 0.f, m[0][3] = 0.f;
		m[1][0] = 0.f, m[1][1] = 1.f, m[1][2] = 0.f, m[1][3] = 0.f;
		m[2][0] = 0.f, m[2][1] = 0.f, m[2][2] = 1.f, m[2][3] = 0.f;

		instances.push_back(
			vk::AccelerationStructureInstanceKHR()
				.setTransform(transform)
				.setInstanceCustomIndex(0)
				.setMask(0xff)
				.setInstanceShaderBindingTableRecordOffset(0)
				.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable)
				.setAccelerationStructureReference(mObjectBLAS[i]->address)
		);
	}

	mTLAS = new zvk::AccelerationStructure(
		mCtx, zvk::QueueIdx::GeneralUse, instances, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
	);
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
	}
	cmd->submitAndWait();
	delete cmd;
}

void PathTracingPass::createShaderBindingTable() {
	// TODO:
	// write a wrapper class for Shader Binding Table

	const auto& ext = mCtx->instance()->extFunctions();
	const auto& pipelineProps = mCtx->instance()->rayTracingPipelineProperties;

	uint32_t numMiss = 1;
	uint32_t numHit = 1;
	uint32_t numHandles = 1 + numMiss + numHit;
	uint32_t handleSize = pipelineProps.shaderGroupHandleSize;

	uint32_t alignedHandleSize = zvk::align(pipelineProps.shaderGroupHandleSize, pipelineProps.shaderGroupHandleAlignment);
	uint32_t baseAlignment = pipelineProps.shaderGroupBaseAlignment;

	mRayGenRegion.stride = zvk::align(alignedHandleSize, baseAlignment);
	mRayGenRegion.size = mRayGenRegion.stride;

	mMissRegion.stride = alignedHandleSize;
	mMissRegion.size = zvk::align(numMiss * alignedHandleSize, baseAlignment);

	mHitRegion.stride = alignedHandleSize;
	mHitRegion.size = zvk::align(numHit * alignedHandleSize, baseAlignment);

	uint32_t dataSize = numHandles * handleSize;
	auto handles = ext.getRayTracingShaderGroupHandlesKHR(mCtx->device, mPipeline, 0, numHandles, dataSize);

	vk::DeviceSize SBTSize = mRayGenRegion.size + mMissRegion.size + mHitRegion.size + mCallRegion.size;

	mShaderBindingTable = zvk::Memory::createBuffer(
		mCtx, SBTSize,
		vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		vk::MemoryAllocateFlagBits::eDeviceAddress
	);
	mShaderBindingTable->mapMemory();

	vk::DeviceAddress SBTAddress = mShaderBindingTable->address();
	mRayGenRegion.deviceAddress = SBTAddress;
	mMissRegion.deviceAddress = SBTAddress + mRayGenRegion.size;
	mHitRegion.deviceAddress = SBTAddress + mRayGenRegion.size + mMissRegion.size;

	auto getHandle = [&](uint32_t i) { return handles.data() + i * handleSize; };

	auto pSBTBuffer = reinterpret_cast<uint8_t*>(mShaderBindingTable->data);
	uint8_t* pData = nullptr;
	uint32_t handleIdx = 0;

	pData = pSBTBuffer;
	memcpy(pData, getHandle(handleIdx++), handleSize);

	pData = pSBTBuffer + mRayGenRegion.size;
	for (uint32_t i = 0; i < numMiss; i++) {
		memcpy(pData, getHandle(handleIdx++), handleSize);
		pData += mMissRegion.stride;
	}

	pData = pSBTBuffer + mRayGenRegion.size + mMissRegion.size;
	for (uint32_t i = 0; i < numHit; i++) {
		memcpy(pData, getHandle(handleIdx++), handleSize);
		pData += mHitRegion.stride;
	}
	mShaderBindingTable->unmapMemory();
}

void PathTracingPass::createDescriptor() {
	std::vector<vk::DescriptorSetLayoutBinding> bindings = {
		zvk::Descriptor::makeBinding(0, vk::DescriptorType::eAccelerationStructureKHR, RayTracingShaderStageFlags)
	};

	mDescriptorSetLayout = new zvk::DescriptorSetLayout(mCtx, bindings);
	mDescriptorPool = new zvk::DescriptorPool(mCtx, { mDescriptorSetLayout }, 1);
	mDescriptorSet = mDescriptorPool->allocDescriptorSet(mDescriptorSetLayout->layout);
}

void PathTracingPass::destroyFrame() {
	for (int i = 0; i < 2; i++) {
		delete colorOutput[i];
		delete reservoir[i];
	}
}
