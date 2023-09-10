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
#include "Scene.h"

#include "util/Error.h"
#include "util/Timer.h"

struct Reservoir {
	uint32_t pad;
};

class PathTracingPass : public zvk::BaseVkObject {
public:
	PathTracingPass(const zvk::Context* ctx, const Resource& resource, const DeviceScene* scene, vk::Extent2D extent, zvk::QueueIdx queueIdx);

	~PathTracingPass() { destroy(); }
	void destroy();

	vk::DescriptorSetLayout descSetLayout() const { return mDescriptorSetLayout->layout; }

	void createPipeline(zvk::ShaderManager* shaderManager, uint32_t maxDepth, const std::vector<vk::DescriptorSetLayout>& descLayouts);

	void render(
		vk::CommandBuffer cmd, vk::DescriptorSet resourceDescSet, vk::DescriptorSet imageOutDescSet,
		vk::Extent2D extent, uint32_t maxDepth);

	void initDescriptor();
	void swap();
	void recreateFrame(vk::Extent2D extent, zvk::QueueIdx queueIdx);

private:
	void createAccelerationStructure(const Resource& resource, const DeviceScene* scene, zvk::QueueIdx queueIdx);
	void createFrame(vk::Extent2D extent, zvk::QueueIdx queueIdx);
	void createShaderBindingTable();
	void createDescriptor();

	void destroyFrame();

public:
	std::unique_ptr<zvk::Image> colorOutput[2];
	std::unique_ptr<zvk::Buffer> reservoir[2];

private:
	vk::Pipeline mPipeline;
	vk::PipelineLayout mPipelineLayout;

	std::vector<std::unique_ptr<zvk::AccelerationStructure>> mObjectBLAS;
	std::unique_ptr<zvk::AccelerationStructure> mTLAS;
	std::unique_ptr<zvk::Buffer> mShaderBindingTable;

	vk::StridedDeviceAddressRegionKHR mRayGenRegion;
	vk::StridedDeviceAddressRegionKHR mMissRegion;
	vk::StridedDeviceAddressRegionKHR mHitRegion;
	vk::StridedDeviceAddressRegionKHR mCallRegion;

	std::unique_ptr<zvk::DescriptorPool> mDescriptorPool;
	std::unique_ptr<zvk::DescriptorSetLayout> mDescriptorSetLayout;
	vk::DescriptorSet mDescriptorSet;
};