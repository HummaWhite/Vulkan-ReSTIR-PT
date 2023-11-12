#pragma once

#include <zvk.hpp>

#include "ComputeRayTrace.h"

struct RayTracingRenderParam;

class PTReSTIR : public zvk::BaseVkObject {
public:
	PTReSTIR(const zvk::Context* ctx) : BaseVkObject(ctx) {}
	~PTReSTIR() { destroy(); }
	void destroy();

	void createPipeline(zvk::ShaderManager* shaderManager, const std::vector<vk::DescriptorSetLayout>& descLayouts);

	void render(vk::CommandBuffer cmd, vk::Extent2D extent, const RayTracingRenderParam& param);

private:
	std::unique_ptr<ComputeRayTrace> mPathTracePass;
	std::unique_ptr<ComputeRayTrace> mSpatialRetracePass;
	std::unique_ptr<ComputeRayTrace> mTemporalRetracePass;
	std::unique_ptr<ComputeRayTrace> mSpatialReusePass;
	std::unique_ptr<ComputeRayTrace> mTemporalReusePass;
	std::unique_ptr<ComputeRayTrace> mMISWeightPass;

	std::unique_ptr<zvk::Image> mSpatialNeighborOffsets;
};