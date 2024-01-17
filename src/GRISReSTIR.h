#pragma once

#include <zvk.hpp>

#include "RayTracing.h"

class GRISReSTIR : public zvk::BaseVkObject {
public:
	enum ShiftType { Reconnection, Replay, Hybrid };

	struct Settings {
		uint32_t shiftType;
		float rrScale;
		uint32_t temporalReuse;
		uint32_t spatialReuse;
		uint32_t cap;
	};

public:
	GRISReSTIR(const zvk::Context* ctx) : BaseVkObject(ctx) {}
	~GRISReSTIR() { destroy(); }
	void destroy();

	void createPipeline(zvk::ShaderManager* shaderManager, const std::vector<vk::DescriptorSetLayout>& descLayouts);
	void render(vk::CommandBuffer cmd, vk::Extent2D extent, const zvk::DescriptorSetBindingMap& descSetBindings);
	void GUI(bool& resetFrame, bool& clearReservoir);

public:
	Settings settings = { Hybrid, 1.f, false, true, 20 };

private:
	std::unique_ptr<RayTracing> mPathTracePass;
	//std::unique_ptr<RayTracing> mRetracePass;
	std::unique_ptr<RayTracing> mTemporalReusePass;
	std::unique_ptr<RayTracing> mSpatialReusePass;
	//std::unique_ptr<RayTracing> mMISWeightPass;
};