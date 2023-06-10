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

#include "core/Instance.h"
#include "core/Context.h"
#include "core/Swapchain.h"
#include "core/ShaderManager.h"
#include "core/Memory.h"
#include "core/Descriptor.h"
#include "Scene.h"

#include "util/Error.h"
#include "util/Timer.h"

struct SwapchainCapabilityDetails {
	vk::SurfaceCapabilitiesKHR capabilities;
	std::vector<vk::SurfaceFormatKHR> formats;
	std::vector<vk::PresentModeKHR> presentModes;
};

class Renderer {
	struct CameraData {
		std140(glm::mat4, model);
		std140(glm::mat4, view);
		std140(glm::mat4, proj);
	};

public:
	Renderer(const std::string& name, int width, int height) :
		mName(name), mWidth(width), mHeight(height) {}

	void exec();

	void setShoudResetSwapchain(bool reset) { mShouldResetSwapchain = reset; }

	constexpr static uint32_t NumFramesConcurrent = 4;

private:
	void initWindow();
	void initVulkan();

	void createDepthImage();
	void createRenderPass();
	void createFramebuffers();
	void createPipeline();

	void initScene();

	void createTextureImage();
	void createVertexBuffer();
	void createIndexBuffer();
	void createUniformBuffers();
	void createDescriptors();
	void createRenderCmdBuffers();
	void createSyncObjects();

	void recordRenderCommands();
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
	vk::PipelineLayout mPipelineLayout;

	std::vector<vk::Framebuffer> mFramebuffers;
	zvk::Image mDepthImage;
	std::vector<zvk::CommandBuffer> mGCTCmdBuffers;

	vk::Semaphore mImageReadySemaphore;
	vk::Semaphore mRenderFinishSemaphore;
	vk::Fence mInFlightFence;

	zvk::Buffer mVertexBuffer;
	zvk::Buffer mIndexBuffer;
	zvk::Buffer mCameraUniforms;

	zvk::DescriptorSetLayout mDescriptorSetLayout;
	zvk::DescriptorPool mDescriptorPool;
	vk::DescriptorSet mDescriptorSet;

	zvk::Image mTextureImage;

	Scene mScene;

	bool mShouldResetSwapchain = false;

	Timer mTimer;
};