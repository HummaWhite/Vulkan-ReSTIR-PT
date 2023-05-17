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

#include "util/Error.h"
#include "VkExtFunctions.h"
#include "ShaderManager.h"
#include "Memory.h"

struct QueueFamilyIndices {
	uint32_t graphicsIndex;
	uint32_t presentIndex;
};

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

	void queryExtensionsAndLayers();
	void createInstance();
	void setupDebugMessenger();

	void createSurface();

	void selectPhysicalDevice();
	bool isDeviceAvailable(vk::PhysicalDevice device);
	std::optional<QueueFamilyIndices> getDeviceQueueFamilyIndices(vk::PhysicalDevice device);
	bool hasDeviceExtension(vk::PhysicalDevice device);

	void createLogicalDevice();
	void createSwapchain();
	SwapchainCapabilityDetails querySwapchainCapability(vk::PhysicalDevice device);
	std::tuple<vk::SurfaceFormatKHR, vk::PresentModeKHR> selectSwapchainFormatAndMode(
		const std::vector<vk::SurfaceFormatKHR>& formats,
		const std::vector<vk::PresentModeKHR>& presentModes);
	vk::Extent2D selectSwapchainExtent(const vk::SurfaceCapabilitiesKHR& capabilities);
	void createSwapchainImageViews();

	void createRenderPass();
	void createDescriptorSetLayout();
	void createPipeline();

	void createFramebuffers();

	void createCommandPool();
	void createVertexBuffer();
	void createIndexBuffer();
	void createCommandBuffer();

	void createSyncObjects();
	void recordCommandBuffer(vk::CommandBuffer cmdBuffer, uint32_t imgIndex);
	void drawFrame();

	void recreateSwapchain();
	void cleanupSwapchain();

	void cleanupVulkan();

private:
	std::string mName;

	GLFWwindow* mMainWindow = nullptr;
	int mWidth;
	int mHeight;

	vk::ApplicationInfo mAppInfo;
	std::vector<char*> mRequiredVkExtensions;
	std::vector<char*> mRequiredVkLayers;

	vk::Instance mInstance;
	zvk::ExtFunctions mExtFunctions;
	bool mEnableValidationLayer = true;
	vk::DebugUtilsMessengerEXT mDebugMessenger;

	std::vector<vk::PhysicalDevice> mPhysicalDevices;
	vk::PhysicalDevice mPhysicalDevice;
	vk::PhysicalDeviceMemoryProperties mPhysicalDeviceMemProps;

	uint32_t mGraphicsQueueFamilyIndex;
	uint32_t mPresentQueueFamilyIndex;
	vk::Queue mGraphicsQueue;
	vk::Queue mPresentQueue;

	vk::Device mDevice;

	vk::SurfaceKHR mSurface;

	vk::SwapchainKHR mSwapchain;
	std::vector<vk::Image> mSwapchainImages;
	vk::Format mSwapchainFormat;
	vk::Extent2D mSwapchainExtent;
	std::vector<vk::ImageView> mSwapchainImageViews;

	vk::Pipeline mGraphicsPipeline;
	zvk::ShaderManager mShaderManager;
	vk::RenderPass mRenderPass;
	vk::DescriptorSetLayout mDescriptorSetLayout;
	vk::PipelineLayout mPipelineLayout;

	std::vector<vk::Framebuffer> mSwapchainFramebuffers;

	vk::CommandPool mCommandPool;
	std::vector<vk::CommandBuffer> mCommandBuffers;

	std::vector<vk::Semaphore> mImageAvailableSemaphores;
	std::vector<vk::Semaphore> mRenderFinishedSemaphores;
	std::vector<vk::Fence> mInFlightFences;
	uint32_t mCurFrameIdx = 0;

	zvk::Buffer mVertexBuffer;
	zvk::Buffer mIndexBuffer;

	bool mShouldResetSwapchain = false;
};