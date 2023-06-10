#include "Renderer.h"

#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__) ) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
	#define VK_NULL_HANDLE_REPLACED nullptr
#else
	#define VK_NULL_HANDLE_REPLACED 0
#endif

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
		mSwapchain = new zvk::Swapchain(mContext, mWidth, mHeight);
		mDevice = mContext->device;

		initScene();
		
		createVertexBuffer();
		createUniformBuffers();
		createIndexBuffer();
		createTextureImage();
		createDescriptors();

		createDepthImage();
		createRenderPass();
		createFramebuffers();
		createPipeline();
		createRenderCmdBuffers();
		createSyncObjects();
	}
	catch (const std::exception& e) {
		Log::bracketLine<0>("Error:" + std::string(e.what()));
		cleanupVulkan();
		glfwSetWindowShouldClose(mMainWindow, true);
	}
}

void Renderer::createRenderPass() {
	auto colorAttachment = vk::AttachmentDescription()
		.setFormat(mSwapchain->format())
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

	auto depthAttachment = vk::AttachmentDescription()
		.setFormat(vk::Format::eD32Sfloat)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

	auto attachments = { colorAttachment, depthAttachment };

	auto colorAttachmentRef = vk::AttachmentReference()
		.setAttachment(0)
		.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

	auto depthAttachmentRef = vk::AttachmentReference()
		.setAttachment(1)
		.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

	auto subpass = vk::SubpassDescription()
		.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
		.setColorAttachments(colorAttachmentRef)
		.setPDepthStencilAttachment(&depthAttachmentRef);

	auto subpassDependency = vk::SubpassDependency()
		.setSrcSubpass(VK_SUBPASS_EXTERNAL)
		.setDstSubpass(0)
		.setSrcStageMask(
			vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests)
		.setDstStageMask(
			vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests)
		.setSrcAccessMask(vk::AccessFlagBits::eNone)
		.setDstAccessMask(
			vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite);

	auto renderPassCreateInfo = vk::RenderPassCreateInfo()
		.setAttachments(attachments)
		.setSubpasses(subpass)
		.setDependencies(subpassDependency);

	mRenderPass = mContext->device.createRenderPass(renderPassCreateInfo);
}

void Renderer::createPipeline() {
	mShaderManager = new zvk::ShaderManager(mContext->device);

	auto vertStageInfo = zvk::ShaderManager::shaderStageCreateInfo(
		mShaderManager->createShaderModule("shaders/test.vert.spv"),
		vk::ShaderStageFlagBits::eVertex);

	auto fragStageInfo = zvk::ShaderManager::shaderStageCreateInfo(
		mShaderManager->createShaderModule("shaders/test.frag.spv"),
		vk::ShaderStageFlagBits::eFragment);

	auto shaderStages = { vertStageInfo, fragStageInfo };

	auto dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

	vk::Viewport viewport(
		0.0f, 0.0f,
		float(mSwapchain->width()), float(mSwapchain->height()),
		0.0f, 1.0f);

	vk::Rect2D scissor({ 0, 0 }, mSwapchain->extent());

	auto viewportStateInfo = vk::PipelineViewportStateCreateInfo()
		.setViewports(viewport)
		.setScissors(scissor);

	auto dynamicStateInfo = vk::PipelineDynamicStateCreateInfo()
		.setDynamicStates(dynamicStates);

	auto vertexBindingDesc = MeshVertex::bindingDescription();
	auto vertexAttribDesc = MeshVertex::attributeDescription();
	auto vertexInputStateInfo = vk::PipelineVertexInputStateCreateInfo()
		.setVertexBindingDescriptions(vertexBindingDesc)
		.setVertexAttributeDescriptions(vertexAttribDesc);

	auto inputAssemblyStateInfo = vk::PipelineInputAssemblyStateCreateInfo()
		.setTopology(vk::PrimitiveTopology::eTriangleList)
		.setPrimitiveRestartEnable(false);

	auto rasterizationStateInfo = vk::PipelineRasterizationStateCreateInfo()
		.setDepthClampEnable(false)
		.setRasterizerDiscardEnable(false)
		.setPolygonMode(vk::PolygonMode::eFill)
		.setLineWidth(1.0f)
		.setCullMode(vk::CullModeFlagBits::eBack)
		.setCullMode(vk::CullModeFlagBits::eNone)
		.setFrontFace(vk::FrontFace::eCounterClockwise)
		.setDepthBiasEnable(false);

	auto multisamplingStateInfo = vk::PipelineMultisampleStateCreateInfo()
		.setSampleShadingEnable(false)
		.setRasterizationSamples(vk::SampleCountFlagBits::e1);

	auto blendAttachment = vk::PipelineColorBlendAttachmentState()
		.setBlendEnable(false)
		.setSrcColorBlendFactor(vk::BlendFactor::eOne)
		.setDstColorBlendFactor(vk::BlendFactor::eZero)
		.setColorBlendOp(vk::BlendOp::eAdd)
		.setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
		.setDstAlphaBlendFactor(vk::BlendFactor::eZero)
		.setAlphaBlendOp(vk::BlendOp::eAdd)
		.setColorWriteMask(
			vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
			vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

	auto colorBlendStateInfo = vk::PipelineColorBlendStateCreateInfo()
		.setLogicOpEnable(false)
		.setLogicOp(vk::LogicOp::eCopy)
		.setAttachments(blendAttachment)
		.setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f });

	auto depthStencilStateInfo = vk::PipelineDepthStencilStateCreateInfo()
		.setDepthTestEnable(true)
		.setDepthWriteEnable(true)
		.setDepthCompareOp(vk::CompareOp::eLess)
		.setDepthBoundsTestEnable(false)
		.setStencilTestEnable(false);

	auto pipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo()
		.setSetLayouts(mDescriptorSetLayout->layout);

	mPipelineLayout = mContext->device.createPipelineLayout(pipelineLayoutCreateInfo);

	auto pipelineCreateInfo = vk::GraphicsPipelineCreateInfo()
		.setStages(shaderStages)
		.setPVertexInputState(&vertexInputStateInfo)
		.setPInputAssemblyState(&inputAssemblyStateInfo)
		.setPViewportState(&viewportStateInfo)
		.setPRasterizationState(&rasterizationStateInfo)
		.setPMultisampleState(&multisamplingStateInfo)
		.setPDepthStencilState(nullptr)
		.setPColorBlendState(&colorBlendStateInfo)
		.setPDynamicState(&dynamicStateInfo)
		.setPDepthStencilState(&depthStencilStateInfo)
		.setLayout(mPipelineLayout)
		.setRenderPass(mRenderPass)
		.setSubpass(0)
		.setBasePipelineHandle(nullptr)
		.setBasePipelineIndex(-1);

	auto result = mContext->device.createGraphicsPipeline(nullptr, pipelineCreateInfo);
	if (result.result != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to create pipeline");
	}
	mGraphicsPipeline = result.value;
}

void Renderer::createFramebuffers() {
	mFramebuffers.resize(mSwapchain->size());

	for (size_t i = 0; i < mFramebuffers.size(); i++) {
		auto attachments = { mSwapchain->imageViews()[i], mDepthImage->imageView };

		auto createInfo = vk::FramebufferCreateInfo()
			.setRenderPass(mRenderPass)
			.setAttachments(attachments)
			.setWidth(mSwapchain->width())
			.setHeight(mSwapchain->height())
			.setLayers(1);

		mFramebuffers[i] = mContext->device.createFramebuffer(createInfo);
	}
}

void Renderer::createDepthImage() {
	mDepthImage = zvk::Memory::createImage2D(
		mContext, mSwapchain->extent(), vk::Format::eD32Sfloat, vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal
	);
	mDepthImage->createImageView();
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

void Renderer::createUniformBuffers() {
	mCameraUniforms = zvk::Memory::createBuffer(
		mContext, sizeof(CameraData),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);
	mCameraUniforms->mapMemory();
}

void Renderer::createDescriptors() {
	std::vector<vk::DescriptorSetLayoutBinding> bindings = {
		zvk::Descriptor::makeBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex),
		zvk::Descriptor::makeBinding(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment),
	};

	mDescriptorSetLayout = new zvk::DescriptorSetLayout(mContext, bindings);
	mDescriptorPool = new zvk::DescriptorPool(mContext, { mDescriptorSetLayout }, 1);
	mDescriptorSet = mDescriptorPool->allocDescriptorSet(mDescriptorSetLayout->layout);

	std::vector<vk::WriteDescriptorSet> updates = {
		zvk::Descriptor::makeWrite(
			mDescriptorSetLayout, mDescriptorSet, 0,
			vk::DescriptorBufferInfo(mCameraUniforms->buffer, 0, mCameraUniforms->size)
		),
		zvk::Descriptor::makeWrite(
			mDescriptorSetLayout, mDescriptorSet, 1,
			vk::DescriptorImageInfo(mTextureImage->sampler, mTextureImage->imageView, mTextureImage->layout)
		),
	};

	mContext->device.updateDescriptorSets(updates, {});
}

void Renderer::createRenderCmdBuffers() {
	mGCTCmdBuffers = zvk::Command::createPrimary(mContext, zvk::QueueIdx::GeneralUse, mSwapchain->size());
}

void Renderer::createSyncObjects() {
	auto fenceCreateInfo = vk::FenceCreateInfo()
		.setFlags(vk::FenceCreateFlagBits::eSignaled);

	mInFlightFence = mContext->device.createFence(fenceCreateInfo);
	mImageReadySemaphore = mContext->device.createSemaphore(vk::SemaphoreCreateInfo());
	mRenderFinishSemaphore = mContext->device.createSemaphore(vk::SemaphoreCreateInfo());
}

void Renderer::recordRenderCommands() {
	for (size_t i = 0; i < mGCTCmdBuffers.size(); i++) {
		auto& cmd = mGCTCmdBuffers[i]->cmd;

		auto beginInfo = vk::CommandBufferBeginInfo()
			.setFlags(vk::CommandBufferUsageFlagBits{})
			.setPInheritanceInfo(nullptr);

		cmd.begin(beginInfo);

		vk::ClearValue clearValues[] = {
			vk::ClearColorValue(0.f, 0.f, 0.f, 1.f),
			vk::ClearDepthStencilValue(1.f, 0.f)
		};

		auto renderPassBeginInfo = vk::RenderPassBeginInfo()
			.setRenderPass(mRenderPass)
			.setFramebuffer(mFramebuffers[i])
			.setRenderArea({ { 0, 0 }, mSwapchain->extent() })
			.setClearValues(clearValues);

		cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mGraphicsPipeline);

		cmd.bindVertexBuffers(0, mVertexBuffer->buffer, vk::DeviceSize(0));
		cmd.bindIndexBuffer(mIndexBuffer->buffer, 0, vk::IndexType::eUint32);

		cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, mSwapchain->width(), mSwapchain->height(), 0.0f, 1.0f));
		cmd.setScissor(0, vk::Rect2D({ 0, 0 }, mSwapchain->extent()));

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, mPipelineLayout, 0, mDescriptorSet, {});

		//cmdBuffer.draw(VertexData.size(), 1, 0, 0);
		cmd.drawIndexed(mScene.resource.indices().size(), 1, 0, 0, 0);

		cmd.endRenderPass();
		cmd.end();
	}
}

void Renderer::drawFrame() {
	if (mContext->device.waitForFences(mInFlightFence, true, UINT64_MAX) != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to wait for any fences");
	}

	auto [acquireRes, imgIndex] = mContext->device.acquireNextImageKHR(
		mSwapchain->swapchain(), UINT64_MAX, mImageReadySemaphore
	);

	if (acquireRes == vk::Result::eErrorOutOfDateKHR) {
		recreateFrames();
		return;
	}
	else if (acquireRes != vk::Result::eSuccess && acquireRes != vk::Result::eSuboptimalKHR) {
		throw std::runtime_error("Failed to acquire image from swapchain");
	}

	mContext->device.resetFences(mInFlightFence);

	//mGCTCmdBuffers[imgIndex].cmd.reset();
	//recordRenderCommands(mGCTCmdBuffers[imgIndex].cmd, imgIndex);

	updateUniformBuffer();

	vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	auto waitSemaphore = mImageReadySemaphore;

	auto submitInfo = vk::SubmitInfo()
		.setCommandBuffers(mGCTCmdBuffers[imgIndex]->cmd)
		.setWaitSemaphores(waitSemaphore)
		.setWaitDstStageMask(waitStage)
		.setSignalSemaphores(mRenderFinishSemaphore);

	mContext->queues[zvk::QueueIdx::GeneralUse].queue.submit(submitInfo, mInFlightFence);

	auto swapchain = mSwapchain->swapchain();
	auto presentInfo = vk::PresentInfoKHR()
		.setWaitSemaphores(mRenderFinishSemaphore)
		.setSwapchains(swapchain)
		.setImageIndices(imgIndex);

	try {
		auto presentRes = mContext->queues[zvk::QueueIdx::Present].queue.presentKHR(presentInfo);
	}
	catch (const vk::SystemError& e) {
		recreateFrames();
	}
	if (mShouldResetSwapchain) {
		recreateFrames();
	}

	//mCurFrameIdx = (mCurFrameIdx + 1) % NumFramesConcurrent;
	/*auto presentRes = mPresentQueue.presentKHR(presentInfo);

	if (presentRes == vk::Result::eErrorOutOfDateKHR ||
		presentRes == vk::Result::eSuboptimalKHR || mShouldResetSwapchain) {
		mShouldResetSwapchain = false;
		recreateSwapchain();
	}
	else if (presentRes != vk::Result::eSuccess) {
		throw std::runtime_error("Unable to present");
	}*/
}

void Renderer::updateUniformBuffer() {
	CameraData data;
	data.model = glm::scale(glm::mat4(1.f), glm::vec3(.3f));
	data.model = glm::rotate(data.model, static_cast<float>(mTimer.get() * 1e-9), glm::vec3(0.f, 0.f, 1.f));
	data.view = glm::lookAt(glm::vec3(2.f), glm::vec3(0.f), glm::vec3(0.f, 0.f, 1.f));
	data.proj = glm::perspective(glm::radians(45.f), float(mSwapchain->width()) / mSwapchain->height(), .1f, 10.f);
	data.proj[1][1] *= -1.f;
	memcpy(mCameraUniforms->data, &data, sizeof(Camera));
}

void Renderer::recreateFrames() {
	int width, height;
	glfwGetFramebufferSize(mMainWindow, &width, &height);

	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(mMainWindow, &width, &height);
		glfwWaitEvents();
	}
	mContext->device.waitIdle();

	cleanupFrames();
	mSwapchain = new zvk::Swapchain(mContext, width, height);
	createFramebuffers();
	createDepthImage();
}

void Renderer::cleanupFrames() {
	for (auto& framebuffer : mFramebuffers) {
		mContext->device.destroyFramebuffer(framebuffer);
	}
	delete mDepthImage;
	delete mSwapchain;
}

void Renderer::cleanupVulkan() {
	cleanupFrames();

	delete mDescriptorSetLayout;
	delete mDescriptorPool;

	delete mCameraUniforms;
	delete mVertexBuffer;
	delete mIndexBuffer;
	delete mTextureImage;

	mContext->device.destroyPipeline(mGraphicsPipeline);
	mContext->device.destroyPipelineLayout(mPipelineLayout);
	mContext->device.destroyRenderPass(mRenderPass);

	mContext->device.destroyFence(mInFlightFence);
	mContext->device.destroySemaphore(mImageReadySemaphore);
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

	mTimer.reset();

	recordRenderCommands();
	
	while (!glfwWindowShouldClose(mMainWindow)) {
		glfwPollEvents();
		drawFrame();
	}
	mContext->device.waitIdle();

	cleanupVulkan();
	glfwDestroyWindow(mMainWindow);
	glfwTerminate();
}
