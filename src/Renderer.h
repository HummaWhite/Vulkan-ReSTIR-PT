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

#include "Instance.h"
#include "Context.h"
#include "Swapchain.h"
#include "ShaderManager.h"
#include "Memory.h"

#include "util/Error.h"
#include "util/Timer.h"

struct SwapchainCapabilityDetails {
	vk::SurfaceCapabilitiesKHR capabilities;
	std::vector<vk::SurfaceFormatKHR> formats;
	std::vector<vk::PresentModeKHR> presentModes;
};

class Renderer {
public:
	Renderer(const std::string& name, int width, int height) :
		mName(name), mWidth(width), mHeight(height) {}

	void exec();

	void setShoudResetSwapchain(bool reset) { mShouldResetSwapchain = reset; }

	constexpr static int NumFramesConcurrent = 2;

private:
	void initWindow();
	void initVulkan();

	void createRenderPass();
	void createDescriptorSetLayout();
	void createPipeline();

	void createFramebuffers();

	void createCommandPool();
	void createImage();
	void createVertexBuffer();
	void createIndexBuffer();
	void createUniformBuffers();
	void createDescriptorPool();
	void createDescriptorSets();
	void createCommandBuffer();

	void createSyncObjects();
	void recordCommandBuffer(vk::CommandBuffer cmdBuffer, uint32_t imgIndex);
	void drawFrame();
	void updateUniformBuffer();

	void recreateFrames();
	void cleanupFrames();

	void cleanupVulkan();

private:
	std::string mName;

	GLFWwindow* mMainWindow = nullptr;
	int mWidth;
	int mHeight;

	vk::ApplicationInfo mAppInfo;
	zvk::Instance mInstance;
	zvk::Context mContext;
	vk::Device mDevice;
	zvk::Swapchain mSwapchain;

	vk::Pipeline mGraphicsPipeline;
	zvk::ShaderManager mShaderManager;
	vk::RenderPass mRenderPass;
	vk::DescriptorSetLayout mDescriptorSetLayout;
	vk::PipelineLayout mPipelineLayout;

	std::vector<vk::Framebuffer> mFramebuffers;

	vk::CommandPool mCommandPool;
	std::vector<vk::CommandBuffer> mCommandBuffers;

	std::vector<vk::Semaphore> mImageAvailableSemaphores;
	std::vector<vk::Semaphore> mRenderFinishedSemaphores;
	std::vector<vk::Fence> mInFlightFences;
	uint32_t mCurFrameIdx = 0;

	zvk::Buffer mVertexBuffer;
	zvk::Buffer mIndexBuffer;
	zvk::Buffer mCameraUniforms;
	vk::DescriptorPool mDescriptorPool;
	std::vector<vk::DescriptorSet> mDescriptorSets;

	bool mShouldResetSwapchain = false;

	Timer mTimer;
};