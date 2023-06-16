#include "Renderer.h"
#include "shader/HostDevice.h"

const std::vector<const char*> DeviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
	VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
	VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
	VK_KHR_RAY_QUERY_EXTENSION_NAME,
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

	glfwMakeContextCurrent(mMainWindow);
}

void Renderer::initVulkan() {
	mAppInfo = vk::ApplicationInfo()
		.setPApplicationName(mName.c_str())
		.setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
		.setPEngineName("None")
		.setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
		.setApiVersion(VK_API_VERSION_1_2);

	try {
		mInstance = new zvk::Instance(mAppInfo, mMainWindow, DeviceExtensions);
		mContext = new zvk::Context(mInstance, DeviceExtensions);
		mSwapchain = new zvk::Swapchain(mContext, mWidth, mHeight, SWAPCHAIN_FORMAT, false);
		mShaderManager = new zvk::ShaderManager(mContext->device);

		initScene();

		std::vector<vk::DescriptorSetLayout> descLayouts;

		mGBufferPass = new GBufferPass(mContext, mSwapchain->extent());
		//mPostProcPass = new PostProcPassComp(mContext, mSwapchain);
		mPostProcPass = new PostProcPassFrag(mContext, mSwapchain);
		
		createVertexBuffer();
		createUniformBuffer();
		createIndexBuffer();
		createTextureImage();

		createDescriptor();

		createPipeline();
		createRenderCmdBuffer();
		createSyncObject();

		initImageLayout();
		initDescriptor();
	}
	catch (const std::exception& e) {
		Log::bracketLine<0>("Error:" + std::string(e.what()));
		cleanupVulkan();
		glfwSetWindowShouldClose(mMainWindow, true);
	}
}

void Renderer::createPipeline() {
	std::vector<vk::DescriptorSetLayout> descLayouts = {
		mGBufferPass->descSetLayout(), mPostProcPass->descSetLayout()
	};
	mGBufferPass->createPipeline(mSwapchain->extent(), mShaderManager, descLayouts);
	//mPostProcPass->createPipeline(mShaderManager, descLayouts);
	mPostProcPass->createPipeline(mShaderManager, mSwapchain->extent(), descLayouts);
}

void Renderer::initScene() {
	mScene.load("res/scene.xml");
}

void Renderer::createTextureImage() {
	auto hostImage = zvk::HostImage::createFromFile("res/texture.jpg", zvk::HostImageType::Int8, 4);

	mTextureImage = zvk::Memory::createTexture2D(
		mContext, hostImage,
		vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		vk::ImageLayout::eShaderReadOnlyOptimal, vk::MemoryPropertyFlagBits::eDeviceLocal
	);
	mTextureImage->createSampler(vk::Filter::eLinear);

	delete hostImage;
}

void Renderer::createVertexBuffer() {
	size_t size = mScene.resource.vertices().size() * sizeof(MeshVertex);
	mVertexBuffer = zvk::Memory::createLocalBuffer(
		mContext, zvk::QueueIdx::GeneralUse, mScene.resource.vertices().data(), size, vk::BufferUsageFlagBits::eVertexBuffer
	);
}

void Renderer::createIndexBuffer() {
	size_t size = mScene.resource.indices().size() * sizeof(uint32_t);
	mIndexBuffer = zvk::Memory::createLocalBuffer(
		mContext, zvk::QueueIdx::GeneralUse, mScene.resource.indices().data(), size, vk::BufferUsageFlagBits::eIndexBuffer
	);
}

void Renderer::createUniformBuffer() {
	mCameraUniforms = zvk::Memory::createBuffer(
		mContext, sizeof(CameraData),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);
	mCameraUniforms->mapMemory();
}

void Renderer::createDescriptor() {
}

void Renderer::initImageLayout() {
	auto cmd = zvk::Command::createOneTimeSubmit(mContext, zvk::QueueIdx::GeneralUse);
	std::vector<vk::ImageMemoryBarrier> barriers;

	for (int i = 0; i < 2; i++) {
		barriers.push_back(
			mGBufferPass->depthNormal[i]->getBarrier(
				vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eNone, vk::AccessFlagBits::eShaderWrite
			)
		);

		barriers.push_back(
			mGBufferPass->albedoMatIdx[i]->getBarrier(
				vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eNone, vk::AccessFlagBits::eShaderWrite
			)
		);
	}
	cmd->cmd.pipelineBarrier(
		vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags{ 0 },
		{}, {}, barriers
	);

	cmd->oneTimeSubmit();
	delete cmd;
}

void Renderer::initDescriptor() {
	mGBufferPass->initDescriptor(mCameraUniforms, mTextureImage);
	//mPostProcPass->updateDescriptor(mGBufferPass->depthNormal, mSwapchain);
	mPostProcPass->initDescriptor(mGBufferPass->depthNormal, mGBufferPass->albedoMatIdx);
}

void Renderer::createRenderCmdBuffer() {
	mGCTCmdBuffers = zvk::Command::createPrimary(mContext, zvk::QueueIdx::GeneralUse, mSwapchain->numImages());
}

void Renderer::createSyncObject() {
	auto fenceCreateInfo = vk::FenceCreateInfo()
		.setFlags(vk::FenceCreateFlagBits::eSignaled);

	mInFlightFence = mContext->device.createFence(fenceCreateInfo);
	mFrameReadySemaphore = mContext->device.createSemaphore(vk::SemaphoreCreateInfo());
	mRenderFinishSemaphore = mContext->device.createSemaphore(vk::SemaphoreCreateInfo());
}

void Renderer::recordRenderCommand(vk::CommandBuffer cmd, uint32_t imageIdx) {
	auto beginInfo = vk::CommandBufferBeginInfo()
		.setFlags(vk::CommandBufferUsageFlagBits{})
		.setPInheritanceInfo(nullptr);

	cmd.begin(beginInfo); {
		mGBufferPass->render(cmd, mVertexBuffer->buffer, mIndexBuffer->buffer, 0, mScene.resource.indices().size(), mSwapchain->extent());
		mPostProcPass->render(cmd, mSwapchain->extent(), imageIdx);
	}
	cmd.end();
}

void Renderer::updateUniformBuffer() {
	CameraData data;
	data.model = glm::scale(glm::mat4(1.f), glm::vec3(.3f));
	data.model = glm::rotate(data.model, static_cast<float>(mRenderTimer.get() * 1e-3), glm::vec3(0.f, 0.f, 1.f));
	data.view = glm::lookAt(glm::vec3(2.f), glm::vec3(0.f), glm::vec3(0.f, 0.f, 1.f));
	data.proj = glm::perspective(glm::radians(45.f), float(mSwapchain->width()) / mSwapchain->height(), .1f, 10.f);
	data.proj[1][1] *= -1.f;
	memcpy(mCameraUniforms->data, &data, sizeof(Camera));
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
	if (mContext->device.waitForFences(mInFlightFence, true, UINT64_MAX) != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to wait for any fences");
	}
	mContext->device.resetFences(mInFlightFence);

	uint32_t imageIdx = acquireFrame(mFrameReadySemaphore);
	mGCTCmdBuffers[imageIdx]->cmd.reset();
	recordRenderCommand(mGCTCmdBuffers[imageIdx]->cmd, imageIdx);

	vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

	auto submitInfo = vk::SubmitInfo()
		.setCommandBuffers(mGCTCmdBuffers[imageIdx]->cmd)
		.setWaitSemaphores(mFrameReadySemaphore)
		.setWaitDstStageMask(waitStage)
		.setSignalSemaphores(mRenderFinishSemaphore);

	mContext->queues[zvk::QueueIdx::GeneralUse].queue.submit(submitInfo, mInFlightFence);
	presentFrame(imageIdx, mRenderFinishSemaphore);
	mGBufferPass->swap();
	mPostProcPass->swap();
}

void Renderer::loop() {
	updateUniformBuffer();
	drawFrame();
}

void Renderer::recreateFrame() {
	int width, height;
	glfwGetFramebufferSize(mMainWindow, &width, &height);

	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(mMainWindow, &width, &height);
		glfwWaitEvents();
	}
	mContext->device.waitIdle();

	delete mSwapchain;
	mSwapchain = new zvk::Swapchain(mContext, width, height, SWAPCHAIN_FORMAT, false);
	mGBufferPass->recreateFrame(mSwapchain->extent());
	mPostProcPass->recreateFrame(mSwapchain);
}

void Renderer::cleanupVulkan() {
	delete mSwapchain;

	delete mDescriptorSetLayout;
	delete mDescriptorPool;

	delete mCameraUniforms;
	delete mVertexBuffer;
	delete mIndexBuffer;
	delete mTextureImage;

	delete mGBufferPass;
	delete mPostProcPass;

	mContext->device.destroyFence(mInFlightFence);
	mContext->device.destroySemaphore(mFrameReadySemaphore);
	mContext->device.destroySemaphore(mRenderFinishSemaphore);

	for (auto cmd : mGCTCmdBuffers) {
		delete cmd;
	}

	delete mShaderManager;
	delete mContext;
	delete mInstance;
}

void Renderer::exec() {
	initWindow();
	initVulkan();
	glfwShowWindow(mMainWindow);

	mRenderTimer.reset();
	size_t frameCount = 0;
	
	while (!glfwWindowShouldClose(mMainWindow)) {
		glfwPollEvents();
		loop();

		double time = mFPSTimer.get();

		if (time > 1000.0) {
			double fps = frameCount / time * 1000.0;
			glfwSetWindowTitle(mMainWindow, (mName + "  FPS: " + std::to_string(fps)).c_str());
			mFPSTimer.reset();
			frameCount = 0;
		}
		frameCount++;
	}
	mContext->device.waitIdle();

	cleanupVulkan();
	glfwDestroyWindow(mMainWindow);
	glfwTerminate();
}
