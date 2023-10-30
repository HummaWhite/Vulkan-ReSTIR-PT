#include "Renderer.h"
#include "WindowInput.h"
#include "shader/HostDevice.h"
#include "core/DebugUtils.h"

#include <sstream>
#include <format>
#include <imgui.h>

const std::vector<const char*> InstanceExtensions{
	//VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
};

const std::vector<const char*> DeviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
	VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
	VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
	VK_KHR_RAY_QUERY_EXTENSION_NAME,
	VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
	VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
};

struct DIReservoir {
	glm::vec3 radiance;
	uint32_t sampleCount;
	glm::vec3 wi;
	float dist;
	float resampleWeight;
	float contribWeight;
	float pHat;
	float pad0;
};

struct GIReservoir {
	glm::vec3 radiance;
	uint32_t sampleCount;
	glm::vec3 visiblePos;
	float resampleWeight;
	glm::vec3 visibleNorm;
	float contribWeight;
	glm::vec3 sampledPos;
	float pHat;
};

struct PTReservoir {
	uint32_t pad;
};

void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
	// Log::bracketLine<0>("Resize to " + std::to_string(width) + "x" + std::to_string(height));
	auto appPtr = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
	appPtr->setShoudResetSwapchain(true);
}

void Renderer::initWindow() {
	if (!glfwInit()) {
		throw "Failed to init GLFW";
	}

	int nMonitors;
	auto monitors = glfwGetMonitors(&nMonitors);
	auto mode = glfwGetVideoMode(monitors[0]);

	// You must hint API otherwise it will fail on creating VkSurface
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_DECORATED, true);
	glfwWindowHint(GLFW_RESIZABLE, true);
	//glfwWindowHint(GLFW_VISIBLE, false);

	mMainWindow = glfwCreateWindow(mWidth, mHeight, mName.c_str(), nullptr, nullptr);
	glfwSetWindowUserPointer(mMainWindow, this);
	glfwSetFramebufferSizeCallback(mMainWindow, framebufferResizeCallback);
	glfwSetKeyCallback(mMainWindow, WindowInput::keyCallback);
	glfwSetCursorPosCallback(mMainWindow, WindowInput::cursorCallback);
	glfwSetScrollCallback(mMainWindow, WindowInput::scrollCallback);
	WindowInput::setCamera(mCamera);

	glfwMakeContextCurrent(mMainWindow);
}

void Renderer::initVulkan() {
	mAppInfo = vk::ApplicationInfo()
		.setPApplicationName(mName.c_str())
		.setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
		.setPEngineName("None")
		.setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
		.setApiVersion(VK_API_VERSION_1_3);

	auto descIndexing = vk::PhysicalDeviceDescriptorIndexingFeatures()
		.setShaderSampledImageArrayNonUniformIndexing(true)
		.setDescriptorBindingVariableDescriptorCount(true)
		.setDescriptorBindingPartiallyBound(true)
		.setRuntimeDescriptorArray(true);

	auto bufferDeviceAddress = vk::PhysicalDeviceBufferDeviceAddressFeatures()
		.setBufferDeviceAddress(true)
		.setPNext(&descIndexing);

	auto rayQuery = vk::PhysicalDeviceRayQueryFeaturesKHR()
		.setRayQuery(true)
		.setPNext(&bufferDeviceAddress);

	auto RTPipeline = vk::PhysicalDeviceRayTracingPipelineFeaturesKHR()
		.setRayTracingPipeline(true)
		.setPNext(&rayQuery);

	auto accelStructure = vk::PhysicalDeviceAccelerationStructureFeaturesKHR()
		.setAccelerationStructure(true)
		.setPNext(&RTPipeline);

	void* featureChain = &accelStructure;

	try {
		mInstance = std::make_unique<zvk::Instance>(mAppInfo, mMainWindow, DeviceExtensions);
		mContext = std::make_unique<zvk::Context>(mInstance.get(), DeviceExtensions, featureChain);
		mSwapchain = std::make_unique<zvk::Swapchain>(mContext.get(), mWidth, mHeight, SWAPCHAIN_FORMAT, false);

		mShaderManager = std::make_unique<zvk::ShaderManager>(mContext->device);
		mGUIManager = std::make_unique<GUIManager>(mContext.get(), mMainWindow, mSwapchain->numImages());

		createCameraBuffer();
		createRayImage();

		initScene();

		mDeviceScene = std::make_unique<DeviceScene>(mContext.get(), mScene, zvk::QueueIdx::GeneralUse);

		mCamera = mScene.camera;
		mCamera.setFilmSize({ mWidth, mHeight });
		mCamera.setPlanes(0.001f, 200.f);
		mPrevCamera = mCamera;

		auto extent = mSwapchain->extent();

		mGBufferPass = std::make_unique<GBufferPass>(mContext.get(), extent, mScene.resource);
		mNaiveDIPass = std::make_unique<NaiveDIPass>(mContext.get());
		mNaiveGIPass = std::make_unique<NaiveGIPass>(mContext.get());
		mResampledDIPass = std::make_unique<ResampledDIPass>(mContext.get());
		mResampledGIPass = std::make_unique<ResampledGIPass>(mContext.get());
		mPostProcPass = std::make_unique<PostProcPassFrag>(mContext.get(), mSwapchain.get());

		mScene.clear();

		Log::newLine();

		createDescriptor();

		createPipeline();
		createCommandBuffer();
		createSyncObject();

		initDescriptor();

		initSettings();
	}
	catch (const std::exception& e) {
		Log::line<0>("Error:" + std::string(e.what()));
		cleanupVulkan();
		glfwSetWindowShouldClose(mMainWindow, true);
	}
	Log::newLine();
}

void Renderer::createPipeline() {
	std::vector<vk::DescriptorSetLayout> descLayouts = {
		mCameraDescLayout->layout,
		mDeviceScene->resourceDescLayout->layout,
		mGBufferPass->descSetLayout(),
		mRayImageDescLayout->layout,
		mDeviceScene->rayTracingDescLayout->layout,
	};
	mGBufferPass->createPipeline(mSwapchain->extent(), mShaderManager.get(), descLayouts);
	mNaiveDIPass->createPipeline(mShaderManager.get(), 2, descLayouts);
	mNaiveGIPass->createPipeline(mShaderManager.get(), 2, descLayouts);
	mResampledDIPass->createPipeline(mShaderManager.get(), 2, descLayouts);
	mResampledGIPass->createPipeline(mShaderManager.get(), 2, descLayouts);
	mPostProcPass->createPipeline(mShaderManager.get(), mSwapchain->extent(), descLayouts);
}

void Renderer::initScene() {
	mScene.load(mSceneFile);
}

void Renderer::createCameraBuffer() {
	for (uint32_t i = 0; i < NumFramesInFlight; i++) {
		mCameraBuffer[i] = zvk::Memory::createBuffer(
			mContext.get(), sizeof(Camera) * 2,
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
		mCameraBuffer[i]->mapMemory();
		zvk::DebugUtils::nameVkObject(mContext->device, mCameraBuffer[i]->buffer, "camera[" + std::to_string(i) + "]");
	}
}

void Renderer::createRayImage() {
	constexpr auto outputFormat = vk::Format::eR16G16B16A16Sfloat;
	const auto extent = mSwapchain->extent();

	for (uint32_t i = 0; i < NumFramesInFlight; i++) {

		mDirectOutput[i] = zvk::Memory::createImage2DAndInitLayout(
			mContext.get(), zvk::QueueIdx::GeneralUse, extent, outputFormat,
			vk::ImageTiling::eOptimal,
			vk::ImageLayout::eGeneral,
			vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		mDirectOutput[i]->createImageView();
		mDirectOutput[i]->createSampler(vk::Filter::eLinear);

		zvk::DebugUtils::nameVkObject(mContext->device, mDirectOutput[i]->image, "directOutput[" + std::to_string(i) + "]");

		mIndirectOutput[i] = zvk::Memory::createImage2DAndInitLayout(
			mContext.get(), zvk::QueueIdx::GeneralUse, extent, outputFormat,
			vk::ImageTiling::eOptimal,
			vk::ImageLayout::eGeneral,
			vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		mIndirectOutput[i]->createImageView();
		mIndirectOutput[i]->createSampler(vk::Filter::eLinear);

		zvk::DebugUtils::nameVkObject(mContext->device, mIndirectOutput[i]->image, "indirectOutput[" + std::to_string(i) + "]");

		vk::DeviceSize DIResvSize = sizeof(DIReservoir) * extent.width * extent.height;
		vk::DeviceSize GIResvSize = sizeof(GIReservoir) * extent.width * extent.height;

		for (int j = 0; j < 2; j++) {
			mDIReservoir[i][j] = zvk::Memory::createBuffer(
				mContext.get(), DIResvSize, vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal
			);
			zvk::DebugUtils::nameVkObject(
				mContext->device, mDIReservoir[i][j]->buffer, "DIReservoir[" + std::to_string(i) + ", " + std::to_string(j) + "]"
			);

			mGIReservoir[i][j] = zvk::Memory::createBuffer(
				mContext.get(), GIResvSize, vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal
			);
			zvk::DebugUtils::nameVkObject(
				mContext->device, mGIReservoir[i][j]->buffer, "GIReservoir[" + std::to_string(i) + ", " + std::to_string(j) + "]"
			);
		}
	}
}

void Renderer::createDescriptor() {
	auto rayImageFlags = RayTracingShaderStageFlags | vk::ShaderStageFlagBits::eFragment;

	std::vector<vk::DescriptorSetLayoutBinding> rayImageBindings = {
		zvk::Descriptor::makeBinding(0, vk::DescriptorType::eStorageImage, rayImageFlags),
		zvk::Descriptor::makeBinding(1, vk::DescriptorType::eStorageImage, rayImageFlags),
		zvk::Descriptor::makeBinding(2, vk::DescriptorType::eCombinedImageSampler, rayImageFlags),
		zvk::Descriptor::makeBinding(3, vk::DescriptorType::eCombinedImageSampler, rayImageFlags),
		zvk::Descriptor::makeBinding(4, vk::DescriptorType::eCombinedImageSampler, rayImageFlags),
		zvk::Descriptor::makeBinding(5, vk::DescriptorType::eCombinedImageSampler, rayImageFlags),
		zvk::Descriptor::makeBinding(6, vk::DescriptorType::eStorageBuffer, RayTracingShaderStageFlags),
		zvk::Descriptor::makeBinding(7, vk::DescriptorType::eStorageBuffer, RayTracingShaderStageFlags),
		zvk::Descriptor::makeBinding(8, vk::DescriptorType::eStorageBuffer, RayTracingShaderStageFlags),
		zvk::Descriptor::makeBinding(9, vk::DescriptorType::eStorageBuffer, RayTracingShaderStageFlags),
		zvk::Descriptor::makeBinding(10, vk::DescriptorType::eCombinedImageSampler, rayImageFlags),
	};
	mRayImageDescLayout = std::make_unique<zvk::DescriptorSetLayout>(mContext.get(), rayImageBindings);

	std::vector<vk::DescriptorSetLayoutBinding> cameraBindings = {
		zvk::Descriptor::makeBinding(
			0, vk::DescriptorType::eUniformBuffer,
			vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute | RayTracingShaderStageFlags
		)
	};
	mCameraDescLayout = std::make_unique<zvk::DescriptorSetLayout>(mContext.get(), cameraBindings);

	zvk::DescriptorLayoutArray descLayouts = { mRayImageDescLayout.get(), mRayImageDescLayout.get(), mCameraDescLayout.get() };
	mDescriptorPool = std::make_unique<zvk::DescriptorPool>(mContext.get(), descLayouts, NumFramesInFlight);

	for (uint32_t i = 0; i < NumFramesInFlight; i++) {
		mRayImageDescSet[i][0] = mDescriptorPool->allocDescriptorSet(mRayImageDescLayout->layout);
		mRayImageDescSet[i][1] = mDescriptorPool->allocDescriptorSet(mRayImageDescLayout->layout);
		zvk::DebugUtils::nameVkObject(mContext->device, mRayImageDescSet[i][0], "rayImageDescSet[" + std::to_string(i) + ", 0]");
		zvk::DebugUtils::nameVkObject(mContext->device, mRayImageDescSet[i][1], "rayImageDescSet[" + std::to_string(i) + ", 1]");

		mCameraDescSet[i] = mDescriptorPool->allocDescriptorSet(mCameraDescLayout->layout);
		zvk::DebugUtils::nameVkObject(mContext->device, mCameraDescSet[i], "cameraDescSet[" + std::to_string(i) + "]");

		Log::line("cameraDescSet[" + std::to_string(i) + "] : " + std::format("0x{:x}", std::bit_cast<uint64_t>(mCameraDescSet[i])));
	}
}

void Renderer::initDescriptor() {
	mDeviceScene->initDescriptor();
	mGBufferPass->initDescriptor();

	zvk::DescriptorWrite update(mContext.get());

	for (uint32_t i = 0; i < NumFramesInFlight; i++) {
		for (int j = 0; j < 2; j++) {
			auto rayImageLayout = mRayImageDescLayout.get();
			auto rayImageSet = mRayImageDescSet[i][j];

			uint32_t thisFrame = j;
			uint32_t lastFrame = j ^ 1;

			update.add(rayImageLayout, rayImageSet, 0, zvk::Descriptor::makeImageInfo(mDirectOutput[i].get()));
			update.add(rayImageLayout, rayImageSet, 1, zvk::Descriptor::makeImageInfo(mIndirectOutput[i].get()));
			update.add(rayImageLayout, rayImageSet, 2, zvk::Descriptor::makeImageInfo(mGBufferPass->depthNormal[i][thisFrame].get()));
			update.add(rayImageLayout, rayImageSet, 3, zvk::Descriptor::makeImageInfo(mGBufferPass->depthNormal[i][lastFrame].get()));
			update.add(rayImageLayout, rayImageSet, 4, zvk::Descriptor::makeImageInfo(mGBufferPass->albedoMatId[i][thisFrame].get()));
			update.add(rayImageLayout, rayImageSet, 5, zvk::Descriptor::makeImageInfo(mGBufferPass->albedoMatId[i][lastFrame].get()));
			update.add(rayImageLayout, rayImageSet, 6, zvk::Descriptor::makeBufferInfo(mDIReservoir[i][thisFrame].get()));
			update.add(rayImageLayout, rayImageSet, 7, zvk::Descriptor::makeBufferInfo(mDIReservoir[i][lastFrame].get()));
			update.add(rayImageLayout, rayImageSet, 8, zvk::Descriptor::makeBufferInfo(mGIReservoir[i][thisFrame].get()));
			update.add(rayImageLayout, rayImageSet, 9, zvk::Descriptor::makeBufferInfo(mGIReservoir[i][lastFrame].get()));
			update.add(rayImageLayout, rayImageSet, 10, zvk::Descriptor::makeImageInfo(mGBufferPass->motionVector[i].get()));
			update.flush();
		}
		update.add(mCameraDescLayout.get(), mCameraDescSet[i], 0, vk::DescriptorBufferInfo(mCameraBuffer[i]->buffer, 0, mCameraBuffer[i]->size));
		update.flush();
	}
}

void Renderer::updateDescriptor() {
}

void Renderer::updateCameraUniform() {
	mCamera.nextFrame(mRng);
	Camera write[] = { mCamera, mPrevCamera };
	memcpy(mCameraBuffer[mInFlightFrameIdx]->data, write, sizeof(write));
	mPrevCamera = mCamera;
}

void Renderer::recreateFrame() {
	int width, height;
	glfwGetFramebufferSize(mMainWindow, &width, &height);

	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(mMainWindow, &width, &height);
		glfwWaitEvents();
	}
	mContext->device.waitIdle();

	mSwapchain = std::make_unique<zvk::Swapchain>(mContext.get(), width, height, SWAPCHAIN_FORMAT, false);
	mGBufferPass->recreateFrame(mSwapchain->extent());
	mPostProcPass->recreateFrame(mSwapchain.get());
}

void Renderer::createCommandBuffer() {
	mGCTCmdBuffers = zvk::Command::createPrimary(mContext.get(), zvk::QueueIdx::GeneralUse, NumFramesInFlight);
}

void Renderer::createSyncObject() {
	auto fenceCreateInfo = vk::FenceCreateInfo()
		.setFlags(vk::FenceCreateFlagBits::eSignaled);

	for (uint32_t i = 0; i < NumFramesInFlight; i++) {
		mInFlightFences[i] = mContext->device.createFence(fenceCreateInfo);
		mFrameReadySemaphores[i] = mContext->device.createSemaphore(vk::SemaphoreCreateInfo());
		mRenderFinishSemaphores[i] = mContext->device.createSemaphore(vk::SemaphoreCreateInfo());
	}
}

void Renderer::recordRenderCommand(vk::CommandBuffer cmd, uint32_t imageIdx) {
	uint32_t curFrame = mCurFrame[mInFlightFrameIdx];

	auto beginInfo = vk::CommandBufferBeginInfo()
		.setFlags(vk::CommandBufferUsageFlagBits{})
		.setPInheritanceInfo(nullptr);

	auto& depthNormal = mGBufferPass->depthNormal[mInFlightFrameIdx][curFrame];
	auto& albedoMatId = mGBufferPass->albedoMatId[mInFlightFrameIdx][curFrame];
	auto& motionVector = mGBufferPass->motionVector[mInFlightFrameIdx];
	auto& directOutput = mDirectOutput[mInFlightFrameIdx];
	auto& indirectOutput = mIndirectOutput[mInFlightFrameIdx];
	auto& directReservoir = mDIReservoir[mInFlightFrameIdx][curFrame];
	auto& indirectReservoir = mGIReservoir[mInFlightFrameIdx][curFrame];

	auto GBufferParam = GBufferRenderParam {
		.cameraDescSet = mCameraDescSet[mInFlightFrameIdx],
		.resourceDescSet = mDeviceScene->resourceDescSet,
		.vertexBuffer = mDeviceScene->vertices->buffer,
		.indexBuffer = mDeviceScene->indices->buffer,
		.offset = 0,
		.count = static_cast<uint32_t>(mScene.resource.meshInstances[Resource::Object].size())
	};

	auto rayTracingParam = RayTracingRenderParam {
		.cameraDescSet = mCameraDescSet[mInFlightFrameIdx],
		.resourceDescSet = mDeviceScene->resourceDescSet,
		.rayImageDescSet = mRayImageDescSet[mInFlightFrameIdx][curFrame],
		.rayTracingDescSet = mDeviceScene->rayTracingDescSet,
		.maxDepth = 1
	};

	auto postProcPushConstant = PostProcPassFrag::PushConstant {
		.toneMapping = static_cast<uint32_t>(mSettings.toneMapping),
		.correctGamma = static_cast<uint32_t>(mSettings.correctGamma),
		.noDirect = (mSettings.directMethod == RayTracingMethod::None),
		.noIndirect = (mSettings.indirectMethod == RayTracingMethod::None)
	};

	cmd.begin(beginInfo); {
		std::string gbufferLabel = "G-buffer Pass [" + std::to_string(mInFlightFrameIdx) + ", " + std::to_string(curFrame) + "]";

		zvk::DebugUtils::cmdBeginLabel(cmd, gbufferLabel, { 1.f, .5f, .3f, 1.f }); {
			mGBufferPass->render(cmd, mSwapchain->extent(), mInFlightFrameIdx, curFrame, GBufferParam);
		}
		zvk::DebugUtils::cmdEndLabel(cmd);

		auto GBufferMemoryBarrier = vk::MemoryBarrier(vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead);

		cmd.pipelineBarrier(
			vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eRayTracingShaderKHR,
			vk::DependencyFlags{ 0 }, GBufferMemoryBarrier, {}, {}
		);

		zvk::DebugUtils::cmdBeginLabel(cmd, "Direct Lighting", { .5f, .3f, 1.f, 1.f }); {
			if (mSettings.directMethod == RayTracingMethod::Naive) {
				mNaiveDIPass->render(cmd, mSwapchain->extent(), rayTracingParam);
			}
			else if (mSettings.directMethod == RayTracingMethod::ResampledDI) {
				mResampledDIPass->render(cmd, mSwapchain->extent(), rayTracingParam);
			}
		}
		zvk::DebugUtils::cmdEndLabel(cmd);

		zvk::DebugUtils::cmdBeginLabel(cmd, "Indirect Lighting", { .6f, .3f, 9.f, 1.f }); {
			if (mSettings.indirectMethod == RayTracingMethod::Naive) {
				mNaiveGIPass->render(cmd, mSwapchain->extent(), rayTracingParam);
			}
			else if (mSettings.indirectMethod == RayTracingMethod::ResampledGI) {
				mResampledGIPass->render(cmd, mSwapchain->extent(), rayTracingParam);
			}
		}
		zvk::DebugUtils::cmdEndLabel(cmd);

		auto rayImageMemoryBarrier = vk::MemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

		cmd.pipelineBarrier(
			vk::PipelineStageFlagBits::eRayTracingShaderKHR, vk::PipelineStageFlagBits::eFragmentShader,
			vk::DependencyFlags{ 0 }, rayImageMemoryBarrier, {}, {}
		);

		zvk::DebugUtils::cmdBeginLabel(cmd, "Post Processing", { .8f, .3f, .5f, 1.f }); {
			mPostProcPass->render(cmd, imageIdx, mRayImageDescSet[mInFlightFrameIdx][curFrame], mSwapchain->extent(), postProcPushConstant);
		}
		zvk::DebugUtils::cmdEndLabel(cmd);

		zvk::DebugUtils::cmdBeginLabel(cmd, "GUI", { .3f, .8f, .5f, 1.f }); {
			mGUIManager->render(cmd, mPostProcPass->framebuffers[imageIdx], mSwapchain->extent());
		}
		zvk::DebugUtils::cmdEndLabel(cmd);
	}
	cmd.end();
}

uint32_t Renderer::acquireFrame(vk::Semaphore signalFrameReady) {
	auto [acquireRes, imageIdx] = mContext->device.acquireNextImageKHR(
		mSwapchain->swapchain(), UINT64_MAX, signalFrameReady
	);

	if (acquireRes != vk::Result::eSuccess && acquireRes != vk::Result::eSuboptimalKHR) {
		throw std::runtime_error("Failed to acquire image from swapchain");
	}
	else if (acquireRes == vk::Result::eErrorOutOfDateKHR) {
		recreateFrame();
	}
	return imageIdx;
}

void Renderer::presentFrame(uint32_t imageIdx, vk::Semaphore waitRenderFinish) {
	auto swapchain = mSwapchain->swapchain();

	auto presentInfo = vk::PresentInfoKHR()
		.setWaitSemaphores(waitRenderFinish)
		.setSwapchains(swapchain)
		.setImageIndices(imageIdx);

	try {
		auto presentRes = mContext->queues[zvk::QueueIdx::Present].queue.presentKHR(presentInfo);
	}
	catch (const vk::SystemError& e) {
		Log::line<0>(e.what());
		recreateFrame();
	}
	if (mShouldResetSwapchain) {
		recreateFrame();
	}
}

void Renderer::drawFrame() {
	if (mContext->device.waitForFences(mInFlightFences[mInFlightFrameIdx], true, UINT64_MAX) != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to wait for any fences");
	}
	mContext->device.resetFences(mInFlightFences[mInFlightFrameIdx]);
	uint32_t imageIdx = acquireFrame(mFrameReadySemaphores[mInFlightFrameIdx]);

	updateCameraUniform();

	mGCTCmdBuffers[mInFlightFrameIdx]->cmd.reset();
	recordRenderCommand(mGCTCmdBuffers[mInFlightFrameIdx]->cmd, imageIdx);

	vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

	auto submitInfo = vk::SubmitInfo()
		.setCommandBuffers(mGCTCmdBuffers[mInFlightFrameIdx]->cmd)
		.setWaitSemaphores(mFrameReadySemaphores[mInFlightFrameIdx])
		.setWaitDstStageMask(waitStage)
		.setSignalSemaphores(mRenderFinishSemaphores[mInFlightFrameIdx]);

	try {
		mContext->queues[zvk::QueueIdx::GeneralUse].queue.submit(submitInfo, mInFlightFences[mInFlightFrameIdx]);
	}
	catch (const std::exception& e) {
		Log::line<0>(e.what());
	}
	presentFrame(imageIdx, mRenderFinishSemaphores[mInFlightFrameIdx]);

	mCurFrame[mInFlightFrameIdx] ^= 1;
	mInFlightFrameIdx = (mInFlightFrameIdx + 1) % NumFramesInFlight;
}

void Renderer::initSettings() {
	mSettings.directMethod = RayTracingMethod::ResampledDI;
	mSettings.indirectMethod = RayTracingMethod::None;
	mSettings.frameLimit = 0;
}

void Renderer::processGUI() {
	bool resetFrame = false;

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Settings")) {
			const char* directMethods[] = { "None", "Naive", "ReSTIR DI" };
			const char* indirectMethods[] = { "None", "Naive", "ReSTIR GI" };

			resetFrame |= ImGui::Combo("Direct Method", &mSettings.directMethod, directMethods, IM_ARRAYSIZE(directMethods));
			resetFrame |= ImGui::Combo("Indirect method", &mSettings.indirectMethod, indirectMethods, IM_ARRAYSIZE(indirectMethods));
			resetFrame |= ImGui::Checkbox("Accumulate", &mSettings.accumulate);

			ImGui::Separator();

			const char* toneMappingMethods[] = { "None", "Filmic", "ACES" };

			ImGui::Combo("Tone mapping", &mSettings.toneMapping, toneMappingMethods, IM_ARRAYSIZE(toneMappingMethods));
			ImGui::Checkbox("Correct gamma", &mSettings.correctGamma);

			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
	if (resetFrame || !mSettings.accumulate) {
		mCamera.update();
	}
}

void Renderer::loop() {
	mGUIManager->beginFrame();
	processGUI();
	drawFrame();
}

void Renderer::cleanupVulkan() {
	mSwapchain.reset();

	for (uint32_t i = 0; i < NumFramesInFlight; i++) {
		mCameraBuffer[i].reset();
		mDirectOutput[i].reset();
		mIndirectOutput[i].reset();

		for (int j = 0; j < 2; j++) {
			mDIReservoir[i][j].reset();
			mGIReservoir[i][j].reset();
		}
	}

	mDeviceScene.reset();
	mGBufferPass.reset();
	mNaiveDIPass.reset();
	mNaiveGIPass.reset();
	mResampledDIPass.reset();
	mResampledGIPass.reset();
	mPostProcPass.reset();

	mCameraDescLayout.reset();
	mRayImageDescLayout.reset();
	mDescriptorPool.reset();

	for (uint32_t i = 0; i < NumFramesInFlight; i++) {
		mContext->device.destroyFence(mInFlightFences[i]);
		mContext->device.destroySemaphore(mFrameReadySemaphores[i]);
		mContext->device.destroySemaphore(mRenderFinishSemaphores[i]);
	}

	for (auto& cmd : mGCTCmdBuffers) {
		cmd.reset();
	}

	mGUIManager.reset();
	mShaderManager.reset();
	mContext.reset();
	mInstance.reset();
}

void Renderer::exec() {
	initWindow();
	initVulkan();
	glfwShowWindow(mMainWindow);

	mRenderTimer.reset();
	size_t frameCount = std::numeric_limits<size_t>::max();
	
	while (!glfwWindowShouldClose(mMainWindow)) {
		double time = mRenderTimer.get();
		double FPSTime = mFPSTimer.get();

		glfwPollEvents();

		if (mSettings.frameLimit != 0 && (time - mLastTime < 1000.f / mSettings.frameLimit)) {
			continue;
		}

		if (frameCount != std::numeric_limits<size_t>::max()) {
			WindowInput::setDeltaTime(time - mLastTime);
			WindowInput::processKeys();
		}
		else {
			frameCount = 0;
		}
		loop();

		if (FPSTime > 1000.0) {
			double fps = frameCount / FPSTime * 1000.0;
			double renderTime = 1000.0 / fps;

			std::stringstream ss;
			ss << mName << std::fixed << std::setprecision(2) << "  FPS: " << fps << ", " << renderTime << " ms";

			glfwSetWindowTitle(mMainWindow,ss.str().c_str());
			mFPSTimer.reset();
			frameCount = 0;
		}
		frameCount++;
		mLastTime = time;
	}
	mContext->device.waitIdle();

	cleanupVulkan();
	glfwDestroyWindow(mMainWindow);
	glfwTerminate();
}
