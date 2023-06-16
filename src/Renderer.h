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
#include "PostProcPassComp.h"
#include "PostProcPassFrag.h"

#include "util/Error.h"
#include "util/Timer.h"

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

	void createPipeline();

	void initScene();

	void createTextureImage();
	void createVertexBuffer();
	void createIndexBuffer();
	void createUniformBuffer();
	void createDescriptor();
	void initImageLayout();
	void initDescriptor();
	void createRenderCmdBuffer();
	void createSyncObject();

	void recordRenderCommand(vk::CommandBuffer cmd, uint32_t imageIdx);
	void updateUniformBuffer();

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

	vk::ApplicationInfo mAppInfo;
	zvk::Instance* mInstance = nullptr;
	zvk::Context* mContext = nullptr;
	zvk::ShaderManager* mShaderManager = nullptr;
	zvk::Swapchain* mSwapchain = nullptr;

	vk::Pipeline mPipeline;
	vk::PipelineLayout mPipelineLayout;

	std::vector<zvk::CommandBuffer*> mGCTCmdBuffers;

	vk::Semaphore mFrameReadySemaphore;
	vk::Semaphore mRenderFinishSemaphore;
	vk::Fence mInFlightFence;

	Scene mScene;

	zvk::Buffer* mVertexBuffer = nullptr;
	zvk::Buffer* mIndexBuffer = nullptr;
	zvk::Buffer* mCameraUniforms = nullptr;
	zvk::Image* mTextureImage = nullptr;

	zvk::DescriptorSetLayout* mDescriptorSetLayout = nullptr;
	zvk::DescriptorPool* mDescriptorPool = nullptr;
	vk::DescriptorSet mDescriptorSet;

	GBufferPass* mGBufferPass = nullptr;
	PostProcPassFrag* mPostProcPass = nullptr;
};