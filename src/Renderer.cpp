#include "Renderer.h"
#include "RenderData.h"

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
		mInstance = zvk::Instance(mAppInfo, mMainWindow, DeviceExtensions);
		mContext = zvk::Context(mInstance, DeviceExtensions);
		mDevice = mContext.device;
		mSwapchain = zvk::Swapchain(mContext, mWidth, mHeight);
		
		createRenderPass();
		createDescriptorSetLayout();
		createPipeline();
		createFramebuffers();
		createVertexBuffer();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createIndexBuffer();
		createTextureImage();
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
		.setFormat(mSwapchain.format())
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

	auto colorAttachmentRef = vk::AttachmentReference()
		.setAttachment(0)
		.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

	auto subpass = vk::SubpassDescription()
		.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
		.setColorAttachments(colorAttachmentRef);

	auto subpassDependency = vk::SubpassDependency()
		.setSrcSubpass(VK_SUBPASS_EXTERNAL)
		.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
		.setSrcAccessMask(vk::AccessFlagBits::eNone)
		.setDstSubpass(0)
		.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
		.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

	auto renderPassCreateInfo = vk::RenderPassCreateInfo()
		.setAttachments(colorAttachment)
		.setSubpasses(subpass)
		.setDependencies(subpassDependency);

	mRenderPass = mContext.device.createRenderPass(renderPassCreateInfo);
}

void Renderer::createDescriptorSetLayout() {
	auto cameraBinding = vk::DescriptorSetLayoutBinding()
		.setBinding(0)
		.setDescriptorType(vk::DescriptorType::eUniformBuffer)
		.setDescriptorCount(1)
		.setStageFlags(vk::ShaderStageFlagBits::eVertex);

	auto createInfo = vk::DescriptorSetLayoutCreateInfo()
		.setBindings(cameraBinding);

	mDescriptorSetLayout = mContext.device.createDescriptorSetLayout(createInfo);
}

void Renderer::createPipeline() {
	mShaderManager = zvk::ShaderManager(mContext.device);

	auto vertStageInfo = zvk::ShaderManager::shaderStageCreateInfo(
		mShaderManager.createShaderModule("shaders/test.vert.spv"),
		vk::ShaderStageFlagBits::eVertex);

	auto fragStageInfo = zvk::ShaderManager::shaderStageCreateInfo(
		mShaderManager.createShaderModule("shaders/test.frag.spv"),
		vk::ShaderStageFlagBits::eFragment);

	auto shaderStages = { vertStageInfo, fragStageInfo };

	auto dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

	vk::Viewport viewport(
		0.0f, 0.0f,
		float(mSwapchain.width()), float(mSwapchain.height()),
		0.0f, 1.0f);

	vk::Rect2D scissor({ 0, 0 }, mSwapchain.extent());

	auto viewportStateInfo = vk::PipelineViewportStateCreateInfo()
		.setViewports(viewport)
		.setScissors(scissor);

	auto dynamicStateInfo = vk::PipelineDynamicStateCreateInfo()
		.setDynamicStates(dynamicStates);

	auto vertexBindingDesc = Vertex::bindingDescription();
	auto vertexAttribDesc = Vertex::attributeDescription();
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

	auto pipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo()
		.setSetLayouts(mDescriptorSetLayout);

	mPipelineLayout = mContext.device.createPipelineLayout(pipelineLayoutCreateInfo);

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
		.setLayout(mPipelineLayout)
		.setRenderPass(mRenderPass)
		.setSubpass(0)
		.setBasePipelineHandle(nullptr)
		.setBasePipelineIndex(-1);

	auto result = mContext.device.createGraphicsPipeline(nullptr, pipelineCreateInfo);
	if (result.result != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to create pipeline");
	}
	mGraphicsPipeline = result.value;
}

void Renderer::createFramebuffers() {
	mFramebuffers.resize(mSwapchain.size());

	for (size_t i = 0; i < mFramebuffers.size(); i++) {
		auto createInfo = vk::FramebufferCreateInfo()
			.setRenderPass(mRenderPass)
			.setAttachments(mSwapchain.imageViews()[i])
			.setWidth(mSwapchain.width())
			.setHeight(mSwapchain.height())
			.setLayers(1);

		mFramebuffers[i] = mContext.device.createFramebuffer(createInfo);
	}
}

void Renderer::createTextureImage() {
	auto hostImage = zvk::HostImage::createFromFile("res/texture.jpg", zvk::HostImageType::Int8, 4);

	mTextureImage = zvk::Memory::createTexture2D(
		mContext, hostImage,
		vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		vk::ImageLayout::eShaderReadOnlyOptimal, vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	mTextureImage.createSampler(vk::Filter::eLinear);

	delete hostImage;
}

void Renderer::createVertexBuffer() {
	size_t size = VertexData.size() * sizeof(Vertex);
	mVertexBuffer = zvk::Memory::createLocalBuffer(
		mContext, zvk::QueueIdx::GeneralUse, VertexData.data(), size, vk::BufferUsageFlagBits::eVertexBuffer
	);
}

void Renderer::createIndexBuffer() {
	size_t size = IndexData.size() * sizeof(uint32_t);
	mIndexBuffer = zvk::Memory::createLocalBuffer(
		mContext, zvk::QueueIdx::GeneralUse, IndexData.data(), size, vk::BufferUsageFlagBits::eIndexBuffer
	);
}

void Renderer::createUniformBuffers() {
	size_t size = sizeof(CameraData);
	mCameraUniforms = zvk::Memory::createBuffer(mContext, size,
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);
	mCameraUniforms.mapMemory();
}

void Renderer::createDescriptorPool() {
	vk::DescriptorPoolSize poolSizes[] = {
		vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1024),
		vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 1024),
		vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1024)
	};

	vk::DescriptorPoolSize size(vk::DescriptorType::eUniformBuffer, NumFramesConcurrent);

	auto createInfo = vk::DescriptorPoolCreateInfo()
		.setPoolSizes(size)
		.setMaxSets(NumFramesConcurrent);

	mDescriptorPool = mContext.device.createDescriptorPool(createInfo);
}

void Renderer::createDescriptorSets() {
	auto allocInfo = vk::DescriptorSetAllocateInfo()
		.setDescriptorPool(mDescriptorPool)
		.setSetLayouts(mDescriptorSetLayout);

	mDescriptorSet = mContext.device.allocateDescriptorSets(allocInfo)[0];

	std::vector<vk::WriteDescriptorSet> updates;

	auto bufferInfo = vk::DescriptorBufferInfo()
		.setBuffer(mCameraUniforms.buffer)
		.setOffset(0)
		.setRange(sizeof(CameraData));

	auto update = vk::WriteDescriptorSet()
		.setDstSet(mDescriptorSet)
		.setDstBinding(0)
		.setDstArrayElement(0)
		.setDescriptorType(vk::DescriptorType::eUniformBuffer)
		.setDescriptorCount(1)
		.setBufferInfo(bufferInfo);

	mContext.device.updateDescriptorSets(update, {});
}

void Renderer::createRenderCmdBuffers() {
	mGCTCmdBuffers = zvk::Command::createPrimary(mContext, zvk::QueueIdx::GeneralUse, mSwapchain.size());
}

void Renderer::createSyncObjects() {
	auto fenceCreateInfo = vk::FenceCreateInfo()
		.setFlags(vk::FenceCreateFlagBits::eSignaled);

	mInFlightFence = mContext.device.createFence(fenceCreateInfo);
	mRenderFinishSemaphore = mContext.device.createSemaphore(vk::SemaphoreCreateInfo());
}

void Renderer::recordRenderCommands() {
	for (size_t i = 0; i < mGCTCmdBuffers.size(); i++) {
		auto& cmd = mGCTCmdBuffers[i].cmd;

		auto beginInfo = vk::CommandBufferBeginInfo()
			.setFlags(vk::CommandBufferUsageFlagBits{})
			.setPInheritanceInfo(nullptr);

		cmd.begin(beginInfo);

		auto clearValue = vk::ClearValue()
			.setColor(vk::ClearColorValue().setFloat32({ 0.0f, 0.0f, 0.0f, 1.0f }));

		auto renderPassBeginInfo = vk::RenderPassBeginInfo()
			.setRenderPass(mRenderPass)
			.setFramebuffer(mFramebuffers[i])
			.setRenderArea({ { 0, 0 }, mSwapchain.extent() })
			.setClearValues(clearValue);

		cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mGraphicsPipeline);

		cmd.bindVertexBuffers(0, mVertexBuffer.buffer, vk::DeviceSize(0));
		cmd.bindIndexBuffer(mIndexBuffer.buffer, 0, vk::IndexType::eUint32);

		cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, mSwapchain.width(), mSwapchain.height(), 0.0f, 1.0f));
		cmd.setScissor(0, vk::Rect2D({ 0, 0 }, mSwapchain.extent()));

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, mPipelineLayout, 0, mDescriptorSet, {});

		//cmdBuffer.draw(VertexData.size(), 1, 0, 0);
		cmd.drawIndexed(IndexData.size(), 1, 0, 0, 0);

		cmd.endRenderPass();
		cmd.end();
	}
}

void Renderer::drawFrame() {
	if (mContext.device.waitForFences(mInFlightFence, true, UINT64_MAX) != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to wait for any fences");
	}

	auto [acquireRes, imgIndex] = mContext.device.acquireNextImageKHR(
		mSwapchain.swapchain(), UINT64_MAX, mSwapchain.readySemaphore()
	);

	if (acquireRes == vk::Result::eErrorOutOfDateKHR) {
		recreateFrames();
		return;
	}
	else if (acquireRes != vk::Result::eSuccess && acquireRes != vk::Result::eSuboptimalKHR) {
		throw std::runtime_error("Failed to acquire image from swapchain");
	}

	mContext.device.resetFences(mInFlightFence);

	//mGCTCmdBuffers[imgIndex].cmd.reset();
	//recordRenderCommands(mGCTCmdBuffers[imgIndex].cmd, imgIndex);

	updateUniformBuffer();

	vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	auto waitSemaphore = mSwapchain.readySemaphore();

	auto submitInfo = vk::SubmitInfo()
		.setCommandBuffers(mGCTCmdBuffers[imgIndex].cmd)
		.setWaitSemaphores(waitSemaphore)
		.setWaitDstStageMask(waitStage)
		.setSignalSemaphores(mRenderFinishSemaphore);

	mContext.queues[zvk::QueueIdx::GeneralUse].queue.submit(submitInfo, mInFlightFence);

	auto swapchain = mSwapchain.swapchain();
	auto presentInfo = vk::PresentInfoKHR()
		.setWaitSemaphores(mRenderFinishSemaphore)
		.setSwapchains(swapchain)
		.setImageIndices(imgIndex);

	try {
		auto presentRes = mContext.queues[zvk::QueueIdx::Present].queue.presentKHR(presentInfo);
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
	data.model = glm::rotate(glm::mat4(1.f), static_cast<float>(mTimer.get() * 1e-9), glm::vec3(0.f, 0.f, 1.f));
	data.view = glm::lookAt(glm::vec3(2.f), glm::vec3(0.f), glm::vec3(0.f, 0.f, 1.f));
	data.proj = glm::perspective(glm::radians(45.f), float(mSwapchain.width()) / mSwapchain.height(), .1f, 10.f);
	data.proj[1][1] *= -1.f;
	memcpy(mCameraUniforms.data, &data, sizeof(CameraData));
}

void Renderer::recreateFrames() {
	int width, height;
	glfwGetFramebufferSize(mMainWindow, &width, &height);

	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(mMainWindow, &width, &height);
		glfwWaitEvents();
	}

	mContext.device.waitIdle();
	cleanupFrames();

	mSwapchain = zvk::Swapchain(mContext, width, height);
	createFramebuffers();
}

void Renderer::cleanupFrames() {
	for (auto& framebuffer : mFramebuffers) {
		mContext.device.destroyFramebuffer(framebuffer);
	}
	mSwapchain.destroy();
}

void Renderer::cleanupVulkan() {
	cleanupFrames();

	mContext.device.destroyDescriptorPool(mDescriptorPool);
	mContext.device.destroyDescriptorSetLayout(mDescriptorSetLayout);

	mCameraUniforms.destroy();
	mVertexBuffer.destroy();
	mIndexBuffer.destroy();
	mTextureImage.destroy();

	mContext.device.destroyPipeline(mGraphicsPipeline);
	mContext.device.destroyPipelineLayout(mPipelineLayout);
	mContext.device.destroyRenderPass(mRenderPass);

	mContext.device.destroyFence(mInFlightFence);
	mContext.device.destroySemaphore(mRenderFinishSemaphore);

	for (auto& cmd : mGCTCmdBuffers) {
		cmd.destroy();
	}

	mShaderManager.destroyShaderModules();
	mContext.destroy();
	mInstance.destroy();
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
	mContext.device.waitIdle();

	cleanupVulkan();
	glfwDestroyWindow(mMainWindow);
	glfwTerminate();
}
