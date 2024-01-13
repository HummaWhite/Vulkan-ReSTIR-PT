#pragma once

#include <zvk.hpp>

#include "RayTracing.h"

class TestReSTIR : public zvk::BaseVkObject {
public:
	enum ShiftType { Reconnection, Replay, Hybrid };
	enum SampleType { Light, BSDF, Both };

	struct Settings {
		uint32_t shiftType;
		uint32_t sampleType;
		uint32_t temporalReuse;
		uint32_t spatialReuse;
	};

public:
	TestReSTIR(const zvk::Context* ctx) : BaseVkObject(ctx) {}
	~TestReSTIR() { destroy(); }
	void destroy();

	void createPipeline(zvk::ShaderManager* shaderManager, const std::vector<vk::DescriptorSetLayout>& descLayouts);
	void render(vk::CommandBuffer cmd, vk::Extent2D extent, const zvk::DescriptorSetBindingMap& descSetBindings);
	void GUI(bool& resetFrame, bool& clearReservoir);

public:
	Settings settings = { Replay, BSDF, 0, 1 };

private:
	std::unique_ptr<RayTracing> mPathTracePass;
	std::unique_ptr<RayTracing> mTemporalReusePass;
	std::unique_ptr<RayTracing> mSpatialReusePass;
};