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
		mSwapchain = zvk::Swapchain(mInstance, mContext, mWidth, mHeight);
		
		createRenderPass();
		createDescriptorSetLayout();
		createPipeline();
		createFramebuffers();
		createCommandPool();
		createVertexBuffer();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createIndexBuffer();
		createCommandBuffer();
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
		auto framebufferCreateInfo = vk::FramebufferCreateInfo()
			.setRenderPass(mRenderPass)
			.setAttachments(mSwapchain.imageViews()[i])
			.setWidth(mSwapchain.width())
			.setHeight(mSwapchain.height())
			.setLayers(1);

		mFramebuffers[i] = mContext.device.createFramebuffer(framebufferCreateInfo);
	}
}

void Renderer::createCommandPool() {
	auto commandPoolCreateInfo = vk::CommandPoolCreateInfo()
		.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
		.setQueueFamilyIndex(mContext.graphicsQueue.familyIdx);

	mCommandPool = mContext.device.createCommandPool(commandPoolCreateInfo);
}

void Renderer::createVertexBuffer() {
	size_t size = VertexData.size() * sizeof(Vertex);
	mVertexBuffer = zvk::Buffer::createDeviceLocal(
		mContext, mCommandPool, VertexData.data(), size, vk::BufferUsageFlagBits::eVertexBuffer
	);
}

void Renderer::createIndexBuffer() {
	size_t size = IndexData.size() * sizeof(uint32_t);
	mIndexBuffer = zvk::Buffer::createDeviceLocal(
		mContext, mCommandPool, IndexData.data(), size, vk::BufferUsageFlagBits::eIndexBuffer
	);
}

void Renderer::createUniformBuffers() {
	size_t size = sizeof(CameraData);
	mCameraUniforms = zvk::Buffer::create(mContext, size,
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);
	mCameraUniforms.mapMemory(mContext);
}

void Renderer::createDescriptorPool() {
	vk::DescriptorPoolSize size(vk::DescriptorType::eUniformBuffer, NumFramesConcurrent);

	auto createInfo = vk::DescriptorPoolCreateInfo()
		.setPoolSizes(size)
		.setMaxSets(NumFramesConcurrent);

	mDescriptorPool = mContext.device.createDescriptorPool(createInfo);
}

void Renderer::createDescriptorSets() {
	std::vector<vk::DescriptorSetLayout> layouts(NumFramesConcurrent, mDescriptorSetLayout);

	auto allocInfo = vk::DescriptorSetAllocateInfo()
		.setDescriptorPool(mDescriptorPool)
		.setSetLayouts(layouts);

	mDescriptorSets = mContext.device.allocateDescriptorSets(allocInfo);

	std::vector<vk::WriteDescriptorSet> updates;

	for (auto& set : mDescriptorSets) {
		auto bufferInfo = vk::DescriptorBufferInfo()
			.setBuffer(mCameraUniforms.buffer)
			.setOffset(0)
			.setRange(sizeof(CameraData));

		updates.push_back(vk::WriteDescriptorSet()
			.setDstSet(set)
			.setDstBinding(0)
			.setDstArrayElement(0)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setBufferInfo(bufferInfo)
		);
	}
	mContext.device.updateDescriptorSets(updates, {});
}

void Renderer::createCommandBuffer() {
	auto allocateInfo = vk::CommandBufferAllocateInfo()
		.setCommandPool(mCommandPool)
		.setLevel(vk::CommandBufferLevel::ePrimary)
		.setCommandBufferCount(mSwapchain.size());

	mCommandBuffers = mContext.device.allocateCommandBuffers(allocateInfo);
}

void Renderer::createSyncObjects() {
	vk::SemaphoreCreateInfo semaphoreCreateInfo;

	auto fenceCreateInfo = vk::FenceCreateInfo()
		.setFlags(vk::FenceCreateFlagBits::eSignaled);

	mImageAvailableSemaphores.resize(NumFramesConcurrent);
	mRenderFinishedSemaphores.resize(NumFramesConcurrent);
	mInFlightFences.resize(NumFramesConcurrent);

	for (size_t i = 0; i < NumFramesConcurrent; i++) {
		mImageAvailableSemaphores[i] = mContext.device.createSemaphore(semaphoreCreateInfo);
		mRenderFinishedSemaphores[i] = mContext.device.createSemaphore(semaphoreCreateInfo);
		mInFlightFences[i] = mContext.device.createFence(fenceCreateInfo);
	}
}

void Renderer::recordCommandBuffer(vk::CommandBuffer cmdBuffer, uint32_t imgIndex) {
	auto beginInfo = vk::CommandBufferBeginInfo()
		.setFlags(vk::CommandBufferUsageFlagBits{})
		.setPInheritanceInfo(nullptr);

	cmdBuffer.begin(beginInfo);

	auto clearValue = vk::ClearValue()
		.setColor(vk::ClearColorValue().setFloat32({ 0.0f, 0.0f, 0.0f, 1.0f }));

	auto renderPassBeginInfo = vk::RenderPassBeginInfo()
		.setRenderPass(mRenderPass)
		.setFramebuffer(mFramebuffers[imgIndex])
		.setRenderArea({ { 0, 0 }, mSwapchain.extent() })
		.setClearValues(clearValue);

	cmdBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

	cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mGraphicsPipeline);

	cmdBuffer.bindVertexBuffers(0, mVertexBuffer.buffer, vk::DeviceSize(0));
	cmdBuffer.bindIndexBuffer(mIndexBuffer.buffer, 0, vk::IndexType::eUint32);

	cmdBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, mSwapchain.width(), mSwapchain.height(), 0.0f, 1.0f));
	cmdBuffer.setScissor(0, vk::Rect2D({ 0, 0 }, mSwapchain.extent()));

	cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, mPipelineLayout, 0, mDescriptorSets[mCurFrameIdx], {});

	//cmdBuffer.draw(VertexData.size(), 1, 0, 0);
	cmdBuffer.drawIndexed(IndexData.size(), 1, 0, 0, 0);

	cmdBuffer.endRenderPass();
	cmdBuffer.end();
}

void Renderer::drawFrame() {
	if (mContext.device.waitForFences(mInFlightFences[mCurFrameIdx], true, UINT64_MAX) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to wait for any fences");

	auto [acquireRes, imgIndex] = mContext.device.acquireNextImageKHR(
		mSwapchain.swapchain(), UINT64_MAX,
		mImageAvailableSemaphores[mCurFrameIdx]
	);

	if (acquireRes == vk::Result::eErrorOutOfDateKHR) {
		recreateFrames();
		return;
	}
	else if (acquireRes != vk::Result::eSuccess && acquireRes != vk::Result::eSuboptimalKHR) {
		throw std::runtime_error("Failed to acquire image from swapchain");
	}

	mContext.device.resetFences(mInFlightFences[mCurFrameIdx]);

	mCommandBuffers[mCurFrameIdx].reset();
	recordCommandBuffer(mCommandBuffers[mCurFrameIdx], imgIndex);

	updateUniformBuffer();

	vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	auto submitInfo = vk::SubmitInfo()
		.setCommandBuffers(mCommandBuffers[mCurFrameIdx])
		.setWaitSemaphores(mImageAvailableSemaphores[mCurFrameIdx])
		.setWaitDstStageMask(waitStage)
		.setSignalSemaphores(mRenderFinishedSemaphores[mCurFrameIdx]);

	mContext.graphicsQueue.queue.submit(submitInfo, mInFlightFences[mCurFrameIdx]);

	auto swapchain = mSwapchain.swapchain();
	auto presentInfo = vk::PresentInfoKHR()
		.setWaitSemaphores(mRenderFinishedSemaphores[mCurFrameIdx])
		.setSwapchains(swapchain)
		.setImageIndices(imgIndex);

	try {
		auto presentRes = mContext.presentQueue.queue.presentKHR(presentInfo);
	}
	catch (const vk::SystemError& e) {
		recreateFrames();
	}
	if (mShouldResetSwapchain) {
		recreateFrames();
	}

	mCurFrameIdx = (mCurFrameIdx + 1) % NumFramesConcurrent;
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
	memcpy(mCameraUniforms.mappedMem, &data, sizeof(CameraData));
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

	mSwapchain = zvk::Swapchain(mInstance, mContext, width, height);
	createFramebuffers();
}

void Renderer::cleanupFrames() {
	for (auto& framebuffer : mFramebuffers) {
		mContext.device.destroyFramebuffer(framebuffer);
	}
	mSwapchain.destroy(mContext);
}

void Renderer::cleanupVulkan() {
	cleanupFrames();

	mContext.device.destroyDescriptorPool(mDescriptorPool);

	mCameraUniforms.destroy(mContext.device);

	mContext.device.destroyDescriptorSetLayout(mDescriptorSetLayout);

	mVertexBuffer.destroy(mContext.device);
	mIndexBuffer.destroy(mContext.device);

	mContext.device.destroyPipeline(mGraphicsPipeline);
	mContext.device.destroyPipelineLayout(mPipelineLayout);

	mContext.device.destroyRenderPass(mRenderPass);

	for (auto& fence : mInFlightFences) {
		mContext.device.destroyFence(fence);
	}
	for (auto& sema : mImageAvailableSemaphores) {
		mContext.device.destroySemaphore(sema);
	}
	for (auto& sema : mRenderFinishedSemaphores) {
		mContext.device.destroySemaphore(sema);
	}

	mContext.device.destroyCommandPool(mCommandPool);

	mShaderManager.destroyShaderModules();

	mContext.destroy();
}

void Renderer::exec() {
	initWindow();
	initVulkan();
	glfwShowWindow(mMainWindow);

	mTimer.reset();
	
	while (!glfwWindowShouldClose(mMainWindow)) {
		glfwPollEvents();
		drawFrame();
	}
	mContext.device.waitIdle();

	cleanupVulkan();
	glfwDestroyWindow(mMainWindow);
	glfwTerminate();
}
