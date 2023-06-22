#include "PathTracePass.h"
#include "shader/HostDevice.h"

PathTracePass::PathTracePass(const zvk::Context* ctx, const DeviceScene* scene, zvk::QueueIdx queueIdx) :
	zvk::BaseVkObject(ctx)
{
	createAccelerationStructure(scene, queueIdx);
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

	vk::TransformMatrixKHR transform;
	auto& m = transform.matrix;

	m[0][0] = 1.f, m[0][1] = 0.f, m[0][2] = 0.f, m[0][3] = 0.f;
	m[1][0] = 0.f, m[1][1] = 1.f, m[1][2] = 0.f, m[1][3] = 0.f;
	m[2][0] = 0.f, m[2][1] = 0.f, m[2][2] = 1.f, m[2][3] = 0.f;

	mBLAS = new zvk::AccelerationStructure(
		mCtx, zvk::QueueIdx::GeneralUse, { triangleData }, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
	);

	mTLAS = new zvk::AccelerationStructure(
		mCtx, zvk::QueueIdx::GeneralUse, mBLAS, transform, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
	);
}

void PathTracePass::createDescriptor() {
}
