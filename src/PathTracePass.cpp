#include "PathTracePass.h"
#include "shader/HostDevice.h"

PathTracePass::PathTracePass(const zvk::Context* ctx, const DeviceScene* scene, vk::Extent2D extent, zvk::QueueIdx queueIdx) :
	zvk::BaseVkObject(ctx)
{
	createAccelerationStructure(scene, queueIdx);
	createFrame(extent);
}

void PathTracePass::destroy() {
	destroyFrame();

	delete mBLAS;
	delete mTLAS;

	delete mDescriptorSetLayout;
	delete mDescriptorPool;

	mCtx->device.destroyPipeline(mPipeline);
	mCtx->device.destroyPipelineLayout(mPipelineLayout);
}

void PathTracePass::render(vk::CommandBuffer cmd, vk::Extent2D extent, uint32_t imageIdx) {
}

void PathTracePass::updateDescriptor() {
	zvk::DescriptorWrite update;

	for (int i = 0; i < 2; i++) {
		update.add(
			mDescriptorSetLayout, mDescriptorSet[i], 0,
			vk::DescriptorImageInfo(colorOutput[i]->sampler, colorOutput[i]->view, colorOutput[i]->layout)
		);
		update.add(
			mDescriptorSetLayout, mDescriptorSet[i], 1,
			vk::DescriptorBufferInfo(reservoir[i]->buffer, 0, reservoir[i]->size)
		);
		update.add(
			mDescriptorSetLayout, mDescriptorSet[i], 2,
			vk::WriteDescriptorSetAccelerationStructureKHR(mTLAS->structure)
		);
	}
	mCtx->device.updateDescriptorSets(update.writes, {});
}

void PathTracePass::swap() {
	std::swap(colorOutput[0], colorOutput[1]);
	std::swap(reservoir[0], reservoir[1]);
	std::swap(mDescriptorSet[0], mDescriptorSet[1]);
}

void PathTracePass::recreateFrame(vk::Extent2D extent) {
	destroyFrame();
	createFrame(extent);
}

void PathTracePass::createPipeline(zvk::ShaderManager* shaderManager, const std::vector<vk::DescriptorSetLayout>& descLayouts) {
}

void PathTracePass::createAccelerationStructure(const DeviceScene* scene, zvk::QueueIdx queueIdx) {
	zvk::AccelerationStructureTriangleMesh triangleData {
		.vertexAddress = scene->vertices->address(),
		.indexAddress = scene->indices->address(),
		.vertexStride = sizeof(MeshVertex),
		.vertexFormat = vk::Format::eR32G32B32Sfloat,
		.indexType = vk::IndexType::eUint32,
		.numVertices = scene->numVertices,
		.numIndices = scene->numIndices
	};

	mBLAS = new zvk::AccelerationStructure(
		mCtx, zvk::QueueIdx::GeneralUse, triangleData, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
	);

	vk::TransformMatrixKHR transform;
	auto& m = transform.matrix;

	m[0][0] = 1.f, m[0][1] = 0.f, m[0][2] = 0.f, m[0][3] = 0.f;
	m[1][0] = 0.f, m[1][1] = 1.f, m[1][2] = 0.f, m[1][3] = 0.f;
	m[2][0] = 0.f, m[2][1] = 0.f, m[2][2] = 1.f, m[2][3] = 0.f;

	auto instance = vk::AccelerationStructureInstanceKHR()
		.setTransform(transform)
		.setInstanceCustomIndex(0)
		.setMask(0xff)
		.setInstanceShaderBindingTableRecordOffset(0)
		.setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable)
		.setAccelerationStructureReference(mBLAS->address);

	mTLAS = new zvk::AccelerationStructure(
		mCtx, zvk::QueueIdx::GeneralUse, instance, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
	);
}

void PathTracePass::createFrame(vk::Extent2D extent) {
	for (int i = 0; i < 2; i++) {
		colorOutput[i] = zvk::Memory::createImage2D(
			mCtx, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		colorOutput[i]->createSampler(vk::Filter::eLinear);
		colorOutput[i]->changeLayout(vk::ImageLayout::eGeneral);

		reservoir[i] = zvk::Memory::createBuffer(
			mCtx, sizeof(Reservoir) * extent.width * extent.height, vk::BufferUsageFlagBits::eStorageBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);
	}
}

void PathTracePass::createShaderBindingTable() {
}

void PathTracePass::createDescriptor() {
	auto stages = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR;

	std::vector<vk::DescriptorSetLayoutBinding> bindings = {
		zvk::Descriptor::makeBinding(0, vk::DescriptorType::eStorageImage, stages),
		zvk::Descriptor::makeBinding(1, vk::DescriptorType::eStorageBuffer, stages),
		zvk::Descriptor::makeBinding(2, vk::DescriptorType::eAccelerationStructureKHR, stages),
	};

	mDescriptorSetLayout = new zvk::DescriptorSetLayout(mCtx, bindings);
	mDescriptorPool = new zvk::DescriptorPool(mCtx, { mDescriptorSetLayout }, 2);
	mDescriptorSet[0] = mDescriptorPool->allocDescriptorSet(mDescriptorSetLayout->layout);
	mDescriptorSet[1] = mDescriptorPool->allocDescriptorSet(mDescriptorSetLayout->layout);
}

void PathTracePass::destroyFrame() {
	for (int i = 0; i < 2; i++) {
		delete colorOutput[i];
		delete reservoir[i];
	}
}
