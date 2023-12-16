#pragma once

#include <zvk.hpp>

#include "RayTracing.h"

class GRISReSTIR : public zvk::BaseVkObject {
public:
	enum ShiftType { Reconnection, Replay, Hybrid };

	struct Settings {
		uint32_t shiftType;
		float rrScale;
	};

public:
	GRISReSTIR(const zvk::Context* ctx) : BaseVkObject(ctx) {}
	~GRISReSTIR() { destroy(); }
	void destroy();

	void createPipeline(zvk::ShaderManager* shaderManager, const std::vector<vk::DescriptorSetLayout>& descLayouts);
	void render(vk::CommandBuffer cmd, vk::Extent2D extent, const zvk::DescriptorSetBindingMap& descSetBindings);
	void GUI(bool& resetFrame, bool& clearReservoir);

public:
	Settings settings = { Reconnection, 1.f };

private:
	std::unique_ptr<RayTracing> mPathTracePass;
	std::unique_ptr<RayTracing> mRetracePass;
	std::unique_ptr<zvk::ComputePipeline> mSpatialReusePass;
	std::unique_ptr<zvk::ComputePipeline> mTemporalReusePass;
	std::unique_ptr<zvk::ComputePipeline> mMISWeightPass;
};