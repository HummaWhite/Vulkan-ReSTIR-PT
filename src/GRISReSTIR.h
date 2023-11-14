#pragma once

#include <zvk.hpp>

struct RayTracingRenderParam;

class GRISReSTIR : public zvk::BaseVkObject {
public:
	enum ShiftType { Reconnection, Hybrid };

	struct Settings {
		uint32_t ShiftType;
	};

public:
	GRISReSTIR(const zvk::Context* ctx) : BaseVkObject(ctx) {}
	~GRISReSTIR() { destroy(); }
	void destroy();

	void createPipeline(zvk::ShaderManager* shaderManager, const std::vector<vk::DescriptorSetLayout>& descLayouts);

	void render(vk::CommandBuffer cmd, vk::Extent2D extent, const RayTracingRenderParam& param);

public:
	Settings settings = {};

private:
	std::unique_ptr<zvk::ComputePipeline> mPathTracePass;
	std::unique_ptr<zvk::ComputePipeline> mSpatialRetracePass;
	std::unique_ptr<zvk::ComputePipeline> mTemporalRetracePass;
	std::unique_ptr<zvk::ComputePipeline> mSpatialReusePass;
	std::unique_ptr<zvk::ComputePipeline> mTemporalReusePass;
	std::unique_ptr<zvk::ComputePipeline> mMISWeightPass;
};