#pragma once

//#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#undef min
#undef max

#include <algorithm>
#include <iostream>
#include <optional>
#include <limits>
#include <set>

#include "core/Context.h"
#include "core/Swapchain.h"
#include "core/ShaderManager.h"
#include "core/Memory.h"
#include "core/Descriptor.h"
#include "core/AccelerationStructure.h"
#include "Scene.h"

#include "util/Error.h"
#include "util/Timer.h"

struct Reservoir {
};

class PathTracePass : public zvk::BaseVkObject {
public:
	PathTracePass(const zvk::Context* ctx, const DeviceScene* scene, vk::Extent2D extent, zvk::QueueIdx queueIdx);

	~PathTracePass() { destroy(); }
	void destroy();

	vk::DescriptorSetLayout descSetLayout() const { return mDescriptorSetLayout->layout; }

	void createPipeline(zvk::ShaderManager* shaderManager, const std::vector<vk::DescriptorSetLayout>& descLayouts);

	void render(vk::CommandBuffer cmd, vk::Extent2D extent, uint32_t imageIdx);
	void updateDescriptor();
	void swap();
	void recreateFrame(vk::Extent2D extent);

private:
	void createAccelerationStructure(const DeviceScene* scene, zvk::QueueIdx queueIdx);
	void createFrame(vk::Extent2D extent);
	void createShaderBindingTable();
	void createDescriptor();

	void destroyFrame();

public:
	zvk::Image* colorOutput[2] = { nullptr };
	zvk::Buffer* reservoir[2] = { nullptr };

private:
	vk::Pipeline mPipeline;
	vk::PipelineLayout mPipelineLayout;

	zvk::AccelerationStructure* mBLAS = nullptr;
	zvk::AccelerationStructure* mTLAS = nullptr;

	zvk::DescriptorPool* mDescriptorPool = nullptr;
	zvk::DescriptorSetLayout* mDescriptorSetLayout = nullptr;
	vk::DescriptorSet mDescriptorSet[2];
};