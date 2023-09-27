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
#include "GBufferPass.h"
#include "PathTracingPass.h"
#include "PostProcPassFrag.h"
#include "shader/HostDevice.h"

#include "util/Error.h"
#include "util/Timer.h"

class Renderer {
public:
	Renderer(const std::string& name, int width, int height) :
		mName(name), mWidth(width), mHeight(height) {}

	void exec();

	void setShoudResetSwapchain(bool reset) { mShouldResetSwapchain = reset; }

private:
	void initWindow();
	void initVulkan();

	void createPipeline();

	void initScene();

	void createDescriptor();
	void initImageLayout();
	void initDescriptor();
	void createRenderCmdBuffer();
	void createSyncObject();

	void recordRenderCommand(vk::CommandBuffer cmd, uint32_t imageIdx);
	void updateCameraUniform();

	uint32_t acquireFrame(vk::Semaphore signalFrameReady);
	void presentFrame(uint32_t imageIdx, vk::Semaphore waitRenderFinish);
	void drawFrame();

	void loop();

	void recreateFrame();

	void cleanupVulkan();

private:
	std::string mName;

	int mWidth, mHeight;
	GLFWwindow* mMainWindow = nullptr;
	bool mShouldResetSwapchain = false;
	Timer mFPSTimer;
	Timer mRenderTimer;
	double mLastTime = 0;
	uint32_t mFrameIndex = 0;

	std::default_random_engine mRng;

	vk::ApplicationInfo mAppInfo;
	std::unique_ptr<zvk::Instance> mInstance;
	std::unique_ptr<zvk::Context> mContext;
	std::unique_ptr<zvk::ShaderManager> mShaderManager;
	std::unique_ptr<zvk::Swapchain> mSwapchain;

	vk::Pipeline mPipeline;
	vk::PipelineLayout mPipelineLayout;

	std::vector<std::unique_ptr<zvk::CommandBuffer>> mGCTCmdBuffers;

	vk::Semaphore mFrameReadySemaphores[NumFramesInFlight];
	vk::Semaphore mRenderFinishSemaphores[NumFramesInFlight];
	vk::Fence mInFlightFences[NumFramesInFlight];

	Scene mScene;
	std::unique_ptr<DeviceScene> mDeviceScene;

	std::unique_ptr<GBufferPass> mGBufferPass;
	std::unique_ptr<PathTracingPass> mPathTracingPass;
	std::unique_ptr<PostProcPassFrag> mPostProcPass;

	std::unique_ptr<zvk::DescriptorPool> mDescriptorPool;
	std::unique_ptr<zvk::DescriptorSetLayout> mImageOutDescLayout;
	vk::DescriptorSet mImageOutDescSet[2];
};