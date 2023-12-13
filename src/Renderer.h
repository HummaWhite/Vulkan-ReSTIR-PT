#pragma once

#include <algorithm>
#include <iostream>
#include <optional>
#include <limits>
#include <set>

#include <zvk.hpp>

#include "util/Timer.h"
#include "shader/HostDevice.h"
#include "Scene.h"
#include "GBufferPass.h"
#include "RayTracingPipelineSimple.h"
#include "DIReSTIR.h"
#include "GIReSTIR.h"
#include "GRISReSTIR.h"
#include "PostProcessFrag.h"
#include "GUIManager.h"

struct RayTracing : public zvk::BaseVkObject {
	enum Mode : uint32_t {
		RayQuery = 0, RayPipeline = 1
	};

	RayTracing(const zvk::Context* ctx) : BaseVkObject(ctx) {}

	void createPipeline(
		zvk::ShaderManager* shaderManager, const File::path& rayQueryShader, const File::path& rayGenShader,
		const std::vector<vk::DescriptorSetLayout>& descLayouts,
		uint32_t pushConstantSize = 0);

	void execute(vk::CommandBuffer cmd, vk::Extent2D extent, const zvk::DescriptorSetBindingMap& descSetBindings, const void* pushConstant = nullptr);
	bool GUI();

	std::unique_ptr<zvk::ComputePipeline> rayQuery;
	std::unique_ptr<RayTracingPipelineSimple> rayPipeline;
	Mode mode = RayQuery;
};

class Renderer {
public:
	struct RayTracingMethod {
		enum _RayTracingMethod {
			None = 0, Naive = 1, ResampledDI = 2, ResampledGI = 2, ResampledPT = 3, VisualizeAS = 3
		};
	};

	struct Settings {
		int directMethod = RayTracingMethod::Naive;
		int indirectMethod = RayTracingMethod::Naive;
		int toneMapping = 1;
		bool correctGamma = true;
		bool accumulate = false;
		float frameLimit = 0;
	};

	Renderer(const std::string& name, int width, int height, const std::string& sceneFile) :
		mName(name), mWidth(width), mHeight(height), mSceneFile(sceneFile) {}

	void exec();

	void setShoudResetSwapchain(bool reset) { mShouldResetSwapchain = reset; }

private:
	void initWindow();
	void initVulkan();

	void createPipeline();

	void initScene();
	void createCameraBuffer();
	void createRayImage();

	void createDescriptor();
	void initDescriptor();
	void updateDescriptor();
	void updateCameraUniform();
	void recreateFrame();

	void createCommandBuffer();
	void createSyncObject();

	void recordRenderCommand(vk::CommandBuffer cmd, uint32_t imageIdx);

	uint32_t acquireFrame(vk::Semaphore signalFrameReady);
	void presentFrame(uint32_t imageIdx, vk::Semaphore waitRenderFinish);
	void drawFrame();

	void initSettings();

	void processGUI();

	void loop();

	void cleanupVulkan();

private:
	std::string mName;

	int mWidth, mHeight;
	int mDisplayWidth, mDisplayHeight;
	GLFWwindow* mMainWindow = nullptr;
	bool mShouldResetSwapchain = false;
	Timer mFPSTimer;
	Timer mRenderTimer;
	double mLastTime = 0;
	uint32_t mInFlightFrameIdx = 0;
	uint32_t mCurFrame[NumFramesInFlight] = { 0 };

	Settings mSettings;

	std::unique_ptr<GUIManager> mGUIManager;

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

	std::string mSceneFile;
	Scene mScene;
	Camera mCamera;
	Camera mPrevCamera;
	std::unique_ptr<DeviceScene> mDeviceScene;
	std::unique_ptr<zvk::Buffer> mCameraBuffer[NumFramesInFlight];

	std::unique_ptr<zvk::Image> mDirectOutput[NumFramesInFlight];
	std::unique_ptr<zvk::Image> mIndirectOutput[NumFramesInFlight];

	std::unique_ptr<zvk::Buffer> mDIReservoir[NumFramesInFlight][2];
	std::unique_ptr<zvk::Buffer> mGIReservoir[NumFramesInFlight][2];
	std::unique_ptr<zvk::Buffer> mGRISReservoir[NumFramesInFlight][2];

	std::unique_ptr<GBufferPass> mGBufferPass;
	std::unique_ptr<RayTracing> mNaiveDIPass;
	std::unique_ptr<RayTracing> mNaiveGIPass;
	std::unique_ptr<RayTracing> mResampledDIPass;
	std::unique_ptr<RayTracing> mResampledGIPass;
	std::unique_ptr<GRISReSTIR> mGRISPass;
	std::unique_ptr<zvk::ComputePipeline> mVisualizeASPass;
	std::unique_ptr<PostProcessFrag> mPostProcessPass;

	std::unique_ptr<zvk::DescriptorPool> mDescriptorPool;
	std::unique_ptr<zvk::DescriptorSetLayout> mRayImageDescLayout;
	std::unique_ptr<zvk::DescriptorSetLayout> mCameraDescLayout;
	vk::DescriptorSet mRayImageDescSet[NumFramesInFlight][2];
	vk::DescriptorSet mCameraDescSet[NumFramesInFlight];
};