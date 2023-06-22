#include "PathTracePass.h"
#include "shader/HostDevice.h"

PathTracePass::PathTracePass(const zvk::Context* ctx, const DeviceScene* scene, zvk::QueueIdx queueIdx) :
	zvk::BaseVkObject(ctx)
{
	createBottomLevelAccelerationStructure(scene, queueIdx);
	createTopLevelAccelerationStructure();
}

void PathTracePass::destroy() {
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
}

void PathTracePass::swap() {
}

void PathTracePass::createPipeline(zvk::ShaderManager* shaderManager, const std::vector<vk::DescriptorSetLayout>& descLayouts) {
}

void PathTracePass::createBottomLevelAccelerationStructure(const DeviceScene* scene, zvk::QueueIdx queueIdx) {
	zvk::AccelerationStructureTriangleData triangleData {
		.vertexAddress = scene->vertices->address(),
		.indexAddress = scene->indices->address(),
		.vertexStride = sizeof(MeshVertex),
		.vertexFormat = vk::Format::eR32G32B32Sfloat,
		.indexType = vk::IndexType::eUint32,
		.numVertices = scene->numVertices,
		.numIndices = scene->numIndices
	};

	mBLAS = new zvk::AccelerationStructure(
		mCtx, zvk::QueueIdx::GeneralUse, { triangleData },
		vk::AccelerationStructureTypeKHR::eBottomLevel, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
	);
}

void PathTracePass::createTopLevelAccelerationStructure() {
}

void PathTracePass::createDescriptor() {
}
