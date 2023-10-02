#pragma once

//#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#undef min
#undef max

#include <algorithm>
#include <iostream>
#include <optional>
#include <memory>
#include <limits>
#include <set>

#include "core/Context.h"
#include "core/Swapchain.h"
#include "core/ShaderManager.h"
#include "core/Memory.h"
#include "core/Descriptor.h"
#include "core/AccelerationStructure.h"
#include "core/ShaderBindingTable.h"
#include "Scene.h"

#include "util/Error.h"
#include "util/Timer.h"

struct Reservoir {
	uint32_t pad;
};

class NaiveDirectIllumination : public zvk::BaseVkObject {
public:
	NaiveDirectIllumination(const zvk::Context* ctx, const Resource& resource, const DeviceScene* scene, vk::Extent2D extent, zvk::QueueIdx queueIdx);

	~NaiveDirectIllumination() { destroy(); }
	void destroy();

	void createPipeline(zvk::ShaderManager* shaderManager, uint32_t maxDepth, const std::vector<vk::DescriptorSetLayout>& descLayouts);

	void render(
		vk::CommandBuffer cmd, uint32_t frameIdx,
		vk::DescriptorSet cameraDescSet, vk::DescriptorSet resourceDescSet, vk::DescriptorSet imageOutDescSet, vk::DescriptorSet rayTracingDescSet,
		vk::Extent2D extent, uint32_t maxDepth);

	void recreateFrame(vk::Extent2D extent, zvk::QueueIdx queueIdx);

private:
	void createFrame(vk::Extent2D extent, zvk::QueueIdx queueIdx);
	void destroyFrame();

public:
	std::unique_ptr<zvk::Image> directOutput[2];
	std::unique_ptr<zvk::Buffer> reservoir[2];

private:
	vk::Pipeline mPipeline;
	vk::PipelineLayout mPipelineLayout;
	std::unique_ptr<zvk::ShaderBindingTable> mShaderBindingTable;
};