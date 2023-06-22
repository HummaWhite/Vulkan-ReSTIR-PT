#include "PathTracePass.h"
#include "shader/HostDevice.h"

PathTracePass::PathTracePass(const zvk::Context* ctx, const DeviceScene* scene) : zvk::BaseVkObject(ctx) {
	createBottomLevelStructure(scene);
	createTopLevelStructure();
}

void PathTracePass::destroy() {
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

void PathTracePass::createBottomLevelStructure(const DeviceScene* scene) {
	auto triangleData = vk::AccelerationStructureGeometryTrianglesDataKHR()
		.setVertexFormat(vk::Format::eR32G32B32Sfloat);

	auto geometryData = vk::AccelerationStructureGeometryDataKHR();

	auto geometry = vk::AccelerationStructureGeometryKHR()
		.setFlags(vk::GeometryFlagBitsKHR::eOpaque)
		.setGeometryType(vk::GeometryTypeKHR::eTriangles);
}

void PathTracePass::createTopLevelStructure() {
}

void PathTracePass::createDescriptor() {
}
