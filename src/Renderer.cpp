#include "Renderer.h"
#include "WindowInput.h"
#include "shader/HostDevice.h"
#include "core/DebugUtils.h"

#include <sstream>

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
	uint32_t pad;
};

struct GIReservoir {
	uint32_t pad;
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
	WindowInput::setCamera(mScene.camera);

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

		createCameraBuffer();
		createRayImage();

		initScene();

		mScene.camera.setFilmSize({ mWidth, mHeight });
		mScene.camera.setPlanes(1e-3f, 500.f);
		mScene.camera.nextFrame(mRng);

		mDeviceScene = std::make_unique<DeviceScene>(mContext.get(), mScene, zvk::QueueIdx::GeneralUse);

		auto extent = mSwapchain->extent();

		mGBufferPass = std::make_unique<GBufferPass>(mContext.get(), extent, mScene.resource);
		mNaiveDIPass = std::make_unique<NaiveDIPass>(mContext.get());
		mNaiveGIPass = std::make_unique<NaiveGIPass>(mContext.get());
		mPostProcPass = std::make_unique<PostProcPassFrag>(mContext.get(), mSwapchain.get());

		Log::newLine();

		createDescriptor();

		createPipeline();
		createCommandBuffer();
		createSyncObject();

		initImageLayout();
		initDescriptor();
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
	mPostProcPass->createPipeline(mShaderManager.get(), mSwapchain->extent(), descLayouts);
}

void Renderer::initScene() {
	mScene.load("res/sponza.xml");
}

void Renderer::createCameraBuffer() {
	for (uint32_t i = 0; i < NumFramesInFlight; i++) {
		mCameraBuffer[i] = zvk::Memory::createBuffer(
			mContext.get(), sizeof(Camera),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
		mCameraBuffer[i]->mapMemory();
		zvk::DebugUtils::nameVkObject(mContext->device, mCameraBuffer[i]->buffer, "camera[" + std::to_string(i) + "]");
	}
}

void Renderer::createRayImage() {
	auto extent = mSwapchain->extent();

	for (uint32_t i = 0; i < NumFramesInFlight; i++) {
		mDirectOutput[i] = zvk::Memory::createImage2D(
			mContext.get(), extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		mDirectOutput[i]->createImageView();
		mDirectOutput[i]->createSampler(vk::Filter::eLinear);

		zvk::DebugUtils::nameVkObject(mContext->device, mDirectOutput[i]->image, "directOutput[" + std::to_string(i) + "]");

		mIndirectOutput[i] = zvk::Memory::createImage2D(
			mContext.get(), extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		mIndirectOutput[i]->createImageView();
		mIndirectOutput[i]->createSampler(vk::Filter::eLinear);

		zvk::DebugUtils::nameVkObject(mContext->device, mDirectOutput[i]->image, "indirectOutput[" + std::to_string(i) + "]");

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

void Renderer::initImageLayout() {
	auto cmd = zvk::Command::createOneTimeSubmit(mContext.get(), zvk::QueueIdx::GeneralUse);

	for (uint32_t i = 0; i < NumFramesInFlight; i++) {
		auto GBufferBarriers = {
			mGBufferPass->GBufferA[i]->getBarrier(
				vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eNone, vk::AccessFlagBits::eShaderWrite
			),
			mGBufferPass->GBufferB[i]->getBarrier(
				vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eNone, vk::AccessFlagBits::eShaderWrite
			),
		};
		auto outputBarriers = {
			mDirectOutput[i]->getBarrier(
				vk::ImageLayout::eGeneral, vk::AccessFlagBits::eNone, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite
			),
			mIndirectOutput[i]->getBarrier(
				vk::ImageLayout::eGeneral, vk::AccessFlagBits::eNone, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite
			),
		};

		cmd->cmd.pipelineBarrier(
			vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eFragmentShader,
			vk::DependencyFlags{ 0 }, {}, {}, GBufferBarriers
		);
		cmd->cmd.pipelineBarrier(
			vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eRayTracingShaderKHR,
			vk::DependencyFlags{ 0 }, {}, {}, outputBarriers
		);
	}

	cmd->submitAndWait();
}

void Renderer::createDescriptor() {
	auto imageOutFlags = RayTracingShaderStageFlags | vk::ShaderStageFlagBits::eFragment;

	std::vector<vk::DescriptorSetLayoutBinding> imageOutBindings = {
		zvk::Descriptor::makeBinding(0, vk::DescriptorType::eCombinedImageSampler, imageOutFlags),
		zvk::Descriptor::makeBinding(1, vk::DescriptorType::eCombinedImageSampler, imageOutFlags),
		zvk::Descriptor::makeBinding(2, vk::DescriptorType::eStorageImage, imageOutFlags),
		zvk::Descriptor::makeBinding(3, vk::DescriptorType::eStorageImage, imageOutFlags),
		zvk::Descriptor::makeBinding(4, vk::DescriptorType::eStorageBuffer, imageOutFlags),
		zvk::Descriptor::makeBinding(5, vk::DescriptorType::eStorageBuffer, imageOutFlags),
		zvk::Descriptor::makeBinding(6, vk::DescriptorType::eStorageBuffer, imageOutFlags),
		zvk::Descriptor::makeBinding(7, vk::DescriptorType::eStorageBuffer, imageOutFlags),
	};
	mRayImageDescLayout = std::make_unique<zvk::DescriptorSetLayout>(mContext.get(), imageOutBindings);

	std::vector<vk::DescriptorSetLayoutBinding> cameraBindings = {
		zvk::Descriptor::makeBinding(
			0, vk::DescriptorType::eUniformBuffer,
			vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute | RayTracingShaderStageFlags
		)
	};
	mCameraDescLayout = std::make_unique<zvk::DescriptorSetLayout>(mContext.get(), cameraBindings);

	mDescriptorPool = std::make_unique<zvk::DescriptorPool>(
		mContext.get(), zvk::DescriptorLayoutArray{ mRayImageDescLayout.get(), mCameraDescLayout.get() }, NumFramesInFlight
	);

	for (uint32_t i = 0; i < NumFramesInFlight; i++) {
		mRayImageDescSet[i] = mDescriptorPool->allocDescriptorSet(mRayImageDescLayout->layout);
		mCameraDescSet[i] = mDescriptorPool->allocDescriptorSet(mCameraDescLayout->layout);
	}
}

void Renderer::initDescriptor() {
	mDeviceScene->initDescriptor();
	mGBufferPass->initDescriptor();

	zvk::DescriptorWrite update;

	for (uint32_t i = 0; i < NumFramesInFlight; i++) {
		update.add(mRayImageDescLayout.get(), mRayImageDescSet[i], 0, zvk::Descriptor::makeImageInfo(mGBufferPass->GBufferA[i].get()));
		update.add(mRayImageDescLayout.get(), mRayImageDescSet[i], 1, zvk::Descriptor::makeImageInfo(mGBufferPass->GBufferB[i].get()));
		update.add(mRayImageDescLayout.get(), mRayImageDescSet[i], 2, zvk::Descriptor::makeImageInfo(mDirectOutput[i].get()));
		update.add(mRayImageDescLayout.get(), mRayImageDescSet[i], 3, zvk::Descriptor::makeImageInfo(mIndirectOutput[i].get()));
		update.add(mRayImageDescLayout.get(), mRayImageDescSet[i], 4, zvk::Descriptor::makeBufferInfo(mDIReservoir[i][0].get()));
		update.add(mRayImageDescLayout.get(), mRayImageDescSet[i], 5, zvk::Descriptor::makeBufferInfo(mDIReservoir[i][1].get()));
		update.add(mRayImageDescLayout.get(), mRayImageDescSet[i], 6, zvk::Descriptor::makeBufferInfo(mGIReservoir[i][0].get()));
		update.add(mRayImageDescLayout.get(), mRayImageDescSet[i], 7, zvk::Descriptor::makeBufferInfo(mGIReservoir[i][1].get()));

		update.add(mCameraDescLayout.get(), mCameraDescSet[i], 0, vk::DescriptorBufferInfo(mCameraBuffer[i]->buffer, 0, mCameraBuffer[i]->size));
	}
	mContext->device.updateDescriptorSets(update.writes, {});
}

void Renderer::updateDescriptor() {
}

void Renderer::updateCameraUniform() {
	memcpy(mCameraBuffer[mFrameIndex]->data, &mScene.camera, sizeof(Camera));
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

	for (int i = 0; i < NumFramesInFlight; i++) {
		mInFlightFences[i] = mContext->device.createFence(fenceCreateInfo);
		mFrameReadySemaphores[i] = mContext->device.createSemaphore(vk::SemaphoreCreateInfo());
		mRenderFinishSemaphores[i] = mContext->device.createSemaphore(vk::SemaphoreCreateInfo());
	}
}

void Renderer::recordRenderCommand(vk::CommandBuffer cmd, uint32_t imageIdx) {
	auto beginInfo = vk::CommandBufferBeginInfo()
		.setFlags(vk::CommandBufferUsageFlagBits{})
		.setPInheritanceInfo(nullptr);

	const auto& GBufferA = mGBufferPass->GBufferA[mFrameIndex];
	const auto& GBufferB = mGBufferPass->GBufferB[mFrameIndex];
	const auto& directOutput = mDirectOutput[mFrameIndex];
	const auto& indirectOutput = mIndirectOutput[mFrameIndex];

	cmd.begin(beginInfo); {
		mGBufferPass->render(
			cmd, mSwapchain->extent(), mFrameIndex,
			mCameraDescSet[mFrameIndex], mDeviceScene->resourceDescSet,
			mDeviceScene->vertices->buffer, mDeviceScene->indices->buffer, 0, mScene.resource.meshInstances().size()
		);

		auto GBufferImageBarriers = {
			GBufferA->getBarrier(GBufferA->layout, vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead),
			GBufferB->getBarrier(GBufferB->layout, vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead),
		};

		cmd.pipelineBarrier(
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::PipelineStageFlagBits::eRayTracingShaderKHR,
			vk::DependencyFlags{ 0 },
			{}, {}, GBufferImageBarriers
		);

		mNaiveDIPass->render(
			cmd, mFrameIndex,
			mCameraDescSet[mFrameIndex], mDeviceScene->resourceDescSet, mRayImageDescSet[mFrameIndex], mDeviceScene->rayTracingDescSet,
			mSwapchain->extent(), 1
		);

		mNaiveGIPass->render(
			cmd, mFrameIndex,
			mCameraDescSet[mFrameIndex], mDeviceScene->resourceDescSet, mRayImageDescSet[mFrameIndex], mDeviceScene->rayTracingDescSet,
			mSwapchain->extent(), 1
		);

		auto rayImageBarriers = {
			directOutput->getBarrier(directOutput->layout, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			indirectOutput->getBarrier(indirectOutput->layout, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};

		cmd.pipelineBarrier(
			vk::PipelineStageFlagBits::eRayTracingShaderKHR,
			vk::PipelineStageFlagBits::eFragmentShader,
			vk::DependencyFlags{ 0 },
			{}, {}, rayImageBarriers
		);

		mPostProcPass->render(cmd, mFrameIndex, imageIdx, mRayImageDescSet[mFrameIndex], mSwapchain->extent());
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
		recreateFrame();
	}
	if (mShouldResetSwapchain) {
		recreateFrame();
	}
}

void Renderer::drawFrame() {
	if (mContext->device.waitForFences(mInFlightFences[mFrameIndex], true, UINT64_MAX) != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to wait for any fences");
	}
	mContext->device.resetFences(mInFlightFences[mFrameIndex]);
	uint32_t imageIdx = acquireFrame(mFrameReadySemaphores[mFrameIndex]);

	updateCameraUniform();

	mGCTCmdBuffers[mFrameIndex]->cmd.reset();
	recordRenderCommand(mGCTCmdBuffers[mFrameIndex]->cmd, imageIdx);

	vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

	auto submitInfo = vk::SubmitInfo()
		.setCommandBuffers(mGCTCmdBuffers[mFrameIndex]->cmd)
		.setWaitSemaphores(mFrameReadySemaphores[mFrameIndex])
		.setWaitDstStageMask(waitStage)
		.setSignalSemaphores(mRenderFinishSemaphores[mFrameIndex]);

	try {
		mContext->queues[zvk::QueueIdx::GeneralUse].queue.submit(submitInfo, mInFlightFences[mFrameIndex]);
	}
	catch (const std::exception& e) {
		Log::line<0>(e.what());
	}
	presentFrame(imageIdx, mRenderFinishSemaphores[mFrameIndex]);
	mFrameIndex = (mFrameIndex + 1) % NumFramesInFlight;
}

void Renderer::loop() {
	drawFrame();
	mScene.camera.nextFrame(mRng);
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
