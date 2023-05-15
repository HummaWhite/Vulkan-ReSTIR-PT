#include "Application.h"

#include "VertexData.h"

#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__) ) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
	#define VK_NULL_HANDLE_REPLACED nullptr
#else
	#define VK_NULL_HANDLE_REPLACED 0
#endif

#define ZVK_DEBUG

#if defined(ZVK_DEBUG)
const std::vector<const char*> ValidationLayers =
{
	"VK_LAYER_KHRONOS_validation"
};
const bool EnableValidationLayer = true;
#else
const std::vector<const char*> ValidationLayers;
const bool EnableValidationLayer = false;
#endif

const std::vector<const char*> DeviceExtensions =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static VKAPI_ATTR VkBool32 VKAPI_CALL zvkDebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	Log::bracketLine<0>("Validation layer: " + std::string(pCallbackData->pMessage));
	return VK_FALSE;
}

vk::DebugUtilsMessengerCreateInfoEXT zvkNormalDebugCreateInfo()
{
	using Severity = vk::DebugUtilsMessageSeverityFlagBitsEXT;
	using MsgType = vk::DebugUtilsMessageTypeFlagBitsEXT;

	auto createInfo = vk::DebugUtilsMessengerCreateInfoEXT()
		.setMessageSeverity(Severity::eWarning | Severity::eError)
		.setMessageType(MsgType::eGeneral | MsgType::eValidation | MsgType::ePerformance)
		.setPfnUserCallback(zvkDebugCallback);

	return createInfo;
}

void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	// Log::bracketLine<0>("Resize to " + std::to_string(width) + "x" + std::to_string(height));
	auto appPtr = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
	appPtr->setShoudResetSwapchain(true);
}

void Application::initWindow()
{
	if (!glfwInit())
		throw "Failed to init GLFW";

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

void Application::initVulkan()
{
	mAppInfo = vk::ApplicationInfo()
		.setPApplicationName(mName.c_str())
		.setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
		.setPEngineName("None")
		.setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
		.setApiVersion(VK_API_VERSION_1_2);

	try
	{
		queryExtensionsAndLayers();
		createInstance();
		setupDebugMessenger();
		createSurface();
		selectPhysicalDevice();
		createLogicalDevice();
		createSwapchain();
		createSwapchainImageViews();
		createRenderPass();
		createPipeline();
		createFramebuffers();
		createCommandPool();
		createVertexBuffer();
		createIndexBuffer();
		createCommandBuffer();
		createSyncObjects();
	}
	catch (const std::exception& e)
	{
		Log::bracketLine<0>("Error:" + std::string(e.what()));
		cleanupVulkan();
		glfwSetWindowShouldClose(mMainWindow, true);
	}
}

void Application::queryExtensionsAndLayers()
{
	auto extensionProps = vk::enumerateInstanceExtensionProperties();
	Log::bracketLine<0>("Vulkan extensions");
	for (const auto& i : extensionProps)
		Log::bracketLine<1>(i.extensionName);

	uint32_t nExtensions;
	auto extensions = glfwGetRequiredInstanceExtensions(&nExtensions);
	mRequiredVkExtensions.resize(nExtensions);
	std::memcpy(mRequiredVkExtensions.data(), extensions, sizeof(char*) * nExtensions);
	if (EnableValidationLayer)
		mRequiredVkExtensions.push_back((char*)VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	Log::bracketLine<0>("Required extensions");
	for (auto extension : mRequiredVkExtensions)
		Log::bracketLine<1>(extension);

	auto layerProps = vk::enumerateInstanceLayerProperties();
	Log::bracketLine<0>("Vulkan layers");
	for (const auto& i : layerProps)
		Log::bracketLine<1>(i.layerName);

	if (!EnableValidationLayer)
		return;
	for (const auto& layerName : ValidationLayers)
	{
		bool found = false;
		for (const auto& prop : layerProps)
		{
			if (!std::strcmp(prop.layerName, layerName))
			{
				found = true;
				break;
			}
		}
		if (!found)
			throw std::runtime_error("Required layer: " + std::string(layerName) + " not supported");
	}
}

void Application::createInstance()
{
	auto debugInfo = zvkNormalDebugCreateInfo();
	auto instanceInfo = vk::InstanceCreateInfo()
		.setPApplicationInfo(&mAppInfo)
		.setPEnabledExtensionNames(mRequiredVkExtensions)
		.setPEnabledLayerNames(ValidationLayers)
		.setPNext(EnableValidationLayer ? &debugInfo : nullptr);

	mInstance = vk::createInstance(instanceInfo);
	mZvkExt = ZvkExt(mInstance);
}

void Application::createSurface()
{
	if (glfwCreateWindowSurface(mInstance, mMainWindow, nullptr,
		reinterpret_cast<VkSurfaceKHR*>(&mSurface)) != VK_SUCCESS)
		throw std::runtime_error("Cannot create surface");
	/*auto createInfo = vk::Win32SurfaceCreateInfoKHR()
		.setHwnd(glfwGetWin32Window(mMainWindow))
		.setHinstance(GetModuleHandle(nullptr));
	mVkSurface = mVkInstance.createWin32SurfaceKHR(createInfo);*/
}

void Application::setupDebugMessenger()
{
	if (!EnableValidationLayer)
		return;
	auto createInfo = zvkNormalDebugCreateInfo();
	//mVkDebugMessenger = mVkInstance.createDebugUtilsMessengerEXT(createInfo);
	mDebugMessenger = mZvkExt.createDebugUtilsMessenger(createInfo);
}

void Application::selectPhysicalDevice()
{
	mPhysicalDevices = mInstance.enumeratePhysicalDevices();
	if (mPhysicalDevices.empty())
		throw std::runtime_error("No physical device found");
	Log::bracketLine<0>(std::to_string(mPhysicalDevices.size()) + " physical device(s) found");
	for (const auto& device : mPhysicalDevices)
	{
		auto props = device.getProperties();
		auto features = device.getFeatures();
		Log::bracketLine<1>("Device " + std::to_string(props.deviceID) + ", " +
			props.deviceName.data());
		if (isDeviceAvailable(device))
			mPhysicalDevice = device;
	}
	if (!mPhysicalDevice)
		throw std::runtime_error("No physical device available");
	Log::bracketLine<0>("Selected device: " + std::string(mPhysicalDevice.getProperties().deviceName.data()));

	auto queueFamilyIndices = *getDeviceQueueFamilyIndices(mPhysicalDevice);
	mGraphicsQueueFamilyIndex = queueFamilyIndices.graphicsIndex;
	mPresentQueueFamilyIndex = queueFamilyIndices.presentIndex;
	mPhysicalDeviceMemProps = mPhysicalDevice.getMemoryProperties();
}

bool Application::isDeviceAvailable(vk::PhysicalDevice device)
{
	if (!getDeviceQueueFamilyIndices(device))
		return false;
	if (!hasDeviceExtension(device))
		return false;
	auto swapchainCapability = querySwapchainCapability(device);
	return swapchainCapability.formats.size() && swapchainCapability.presentModes.size();
}

std::optional<QueueFamilyIndices> Application::getDeviceQueueFamilyIndices(vk::PhysicalDevice device)
{
	auto queueFamilyProps = device.getQueueFamilyProperties();
	std::optional<uint32_t> graphics, present;
	for (size_t i = 0; i < queueFamilyProps.size(); i++)
	{
		if (queueFamilyProps[i].queueFlags & vk::QueueFlagBits::eGraphics)
			graphics = i;
		if (device.getSurfaceSupportKHR(i, mSurface))
			present = i;
		if (graphics && present)
			return QueueFamilyIndices{ *graphics, *present };
	};
	return std::nullopt;
}

bool Application::hasDeviceExtension(vk::PhysicalDevice device)
{
	auto extensionProps = device.enumerateDeviceExtensionProperties();
	std::set<std::string> requiredExtensions(DeviceExtensions.begin(), DeviceExtensions.end());
	for (const auto& e : extensionProps)
		requiredExtensions.erase(e.extensionName);
	return requiredExtensions.empty();
}

void Application::createLogicalDevice()
{
	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { mGraphicsQueueFamilyIndex, mPresentQueueFamilyIndex };
	for (const auto& i : uniqueQueueFamilies)
	{
		const auto prior = { 1.0f };
		auto createInfo = vk::DeviceQueueCreateInfo()
			.setQueueFamilyIndex(i)
			.setQueuePriorities(prior);
		queueCreateInfos.push_back(createInfo);
	}
	auto deviceFeatures = vk::PhysicalDeviceFeatures();

	auto deviceCreateInfo = vk::DeviceCreateInfo()
		.setQueueCreateInfos(queueCreateInfos)
		.setPEnabledFeatures(&deviceFeatures)
		.setPEnabledLayerNames(ValidationLayers)
		.setPEnabledExtensionNames(DeviceExtensions);

	mDevice = mPhysicalDevice.createDevice(deviceCreateInfo);
	mGraphicsQueue = mDevice.getQueue(mGraphicsQueueFamilyIndex, 0);
	mPresentQueue = mDevice.getQueue(mPresentQueueFamilyIndex, 0);
}

void Application::createSwapchain()
{
	auto [capabilities, formats, presentModes] = querySwapchainCapability(mPhysicalDevice);
	auto [format, presentMode] = selectSwapchainFormatAndMode(formats, presentModes);
	auto extent = selectSwapchainExtent(capabilities);

	uint32_t nImages = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount)
		nImages = std::min(nImages, capabilities.maxImageCount);

	bool sameQueue = mGraphicsQueueFamilyIndex == mPresentQueueFamilyIndex;
	auto sharingMode = sameQueue ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent;
	auto queueFamilyIndices = sameQueue ? std::vector<uint32_t>() :
		std::vector<uint32_t>({ mGraphicsQueueFamilyIndex, mPresentQueueFamilyIndex });

	auto createInfo = vk::SwapchainCreateInfoKHR()
		.setSurface(mSurface)
		.setMinImageCount(nImages)
		.setImageFormat(format.format)
		.setImageColorSpace(format.colorSpace)
		.setImageExtent(extent)
		.setImageArrayLayers(1)
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
		.setImageSharingMode(sharingMode)
		.setQueueFamilyIndices(queueFamilyIndices)
		.setPreTransform(capabilities.currentTransform)
		.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
		.setPresentMode(presentMode)
		.setClipped(true);

	mSwapchain = mDevice.createSwapchainKHR(createInfo);
	mSwapchainImages = mDevice.getSwapchainImagesKHR(mSwapchain);
	mSwapchainFormat = format.format;
	mSwapchainExtent = extent;
}

SwapchainCapabilityDetails Application::querySwapchainCapability(vk::PhysicalDevice device)
{
	auto capabilities = device.getSurfaceCapabilitiesKHR(mSurface);
	auto formats = device.getSurfaceFormatsKHR(mSurface);
	auto presentModes = device.getSurfacePresentModesKHR(mSurface);
	return { capabilities, formats, presentModes };
}

std::tuple<vk::SurfaceFormatKHR, vk::PresentModeKHR> Application::selectSwapchainFormatAndMode(
	const std::vector<vk::SurfaceFormatKHR>& formats, const std::vector<vk::PresentModeKHR>& presentModes)
{
	vk::SurfaceFormatKHR format;
	vk::PresentModeKHR presentMode;

	for (const auto& i : formats)
	{
		if (i.format == vk::Format::eB8G8R8A8Srgb && i.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
			format = i;
	}
	format = formats[0];

	for (const auto& i : presentModes)
	{
		if (i == vk::PresentModeKHR::eMailbox)
			presentMode = i;
	}
	presentMode = vk::PresentModeKHR::eFifo;
	return { format, presentMode };
}

vk::Extent2D Application::selectSwapchainExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != UINT32_MAX)
		return capabilities.currentExtent;

	int width, height;
	glfwGetFramebufferSize(mMainWindow, &width, &height);

	vk::Extent2D extent(width, height);
	extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
		capabilities.maxImageExtent.width);
	extent.height = std::clamp(extent.height, capabilities.minImageExtent.height,
		capabilities.maxImageExtent.height);
	return extent;
}

void Application::createSwapchainImageViews()
{
	mSwapchainImageViews.resize(mSwapchainImages.size());
	for (size_t i = 0; i < mSwapchainImages.size(); i++)
	{
		auto createInfo = vk::ImageViewCreateInfo()
			.setImage(mSwapchainImages[i])
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(mSwapchainFormat)
			.setComponents(vk::ComponentMapping())
			.setSubresourceRange(
				vk::ImageSubresourceRange()
				.setAspectMask(vk::ImageAspectFlagBits::eColor)
				.setBaseMipLevel(0)
				.setLevelCount(1)
				.setBaseArrayLayer(0)
				.setLayerCount(1));
		mSwapchainImageViews[i] = mDevice.createImageView(createInfo);
	}
}

void Application::createRenderPass()
{
	auto colorAttachment = vk::AttachmentDescription()
		.setFormat(mSwapchainFormat)
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

	mRenderPass = mDevice.createRenderPass(renderPassCreateInfo);
}

void Application::createPipeline()
{
	mShaderManager = ShaderManager(mDevice);

	auto vertStageInfo = ShaderManager::shaderStageCreateInfo(
		mShaderManager.createShaderModule("src/shader/test_vert.spv"),
		vk::ShaderStageFlagBits::eVertex);

	auto fragStageInfo = ShaderManager::shaderStageCreateInfo(
		mShaderManager.createShaderModule("src/shader/test_frag.spv"),
		vk::ShaderStageFlagBits::eFragment);

	auto shaderStages = { vertStageInfo, fragStageInfo };

	auto dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

	vk::Viewport viewport(
		0.0f, 0.0f,
		float(mSwapchainExtent.width), float(mSwapchainExtent.height),
		0.0f, 1.0f);

	vk::Rect2D scissor({ 0, 0 }, mSwapchainExtent);

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
		.setFrontFace(vk::FrontFace::eClockwise)
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
		.setSetLayouts({})
		.setPushConstantRanges({});

	mPipelineLayout = mDevice.createPipelineLayout(pipelineLayoutCreateInfo);

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

	auto result = mDevice.createGraphicsPipeline(nullptr, pipelineCreateInfo);
	if (result.result != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create pipeline");
	mGraphicsPipeline = result.value;
}

void Application::createFramebuffers()
{
	mSwapchainFramebuffers.resize(mSwapchainImages.size());

	for (size_t i = 0; i < mSwapchainFramebuffers.size(); i++)
	{
		auto framebufferCreateInfo = vk::FramebufferCreateInfo()
			.setRenderPass(mRenderPass)
			.setAttachments(mSwapchainImageViews[i])
			.setWidth(mSwapchainExtent.width)
			.setHeight(mSwapchainExtent.height)
			.setLayers(1);

		mSwapchainFramebuffers[i] = mDevice.createFramebuffer(framebufferCreateInfo);
	}
}

void Application::createCommandPool()
{
	auto commandPoolCreateInfo = vk::CommandPoolCreateInfo()
		.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
		.setQueueFamilyIndex(mGraphicsQueueFamilyIndex);

	mCommandPool = mDevice.createCommandPool(commandPoolCreateInfo);
}

void Application::createVertexBuffer()
{
	size_t size = VertexData.size() * sizeof(Vertex);

	auto [buffer, memory] = createDeviceLocalBufferMemory(
		mDevice, mPhysicalDevice, mCommandPool, mGraphicsQueue,
		VertexData.data(), size, vk::BufferUsageFlagBits::eVertexBuffer);

	mVertexBuffer = buffer;
	mVertexBufferMemory = memory;
}

void Application::createIndexBuffer()
{
	size_t size = IndexData.size() * sizeof(uint32_t);

	auto [buffer, memory] = createDeviceLocalBufferMemory(
		mDevice, mPhysicalDevice, mCommandPool, mGraphicsQueue,
		IndexData.data(), size, vk::BufferUsageFlagBits::eIndexBuffer);

	mIndexBuffer = buffer;
	mIndexBufferMemory = memory;
}

void Application::createCommandBuffer()
{
	auto allocateInfo = vk::CommandBufferAllocateInfo()
		.setCommandPool(mCommandPool)
		.setLevel(vk::CommandBufferLevel::ePrimary)
		.setCommandBufferCount(mSwapchainImages.size());

	mCommandBuffers = mDevice.allocateCommandBuffers(allocateInfo);
}

void Application::createSyncObjects()
{
	vk::SemaphoreCreateInfo semaphoreCreateInfo;

	auto fenceCreateInfo = vk::FenceCreateInfo()
		.setFlags(vk::FenceCreateFlagBits::eSignaled);

	mImageAvailableSemaphores.resize(NumFramesConcurrent);
	mRenderFinishedSemaphores.resize(NumFramesConcurrent);
	mInFlightFences.resize(NumFramesConcurrent);

	for (size_t i = 0; i < NumFramesConcurrent; i++)
	{
		mImageAvailableSemaphores[i] = mDevice.createSemaphore(semaphoreCreateInfo);
		mRenderFinishedSemaphores[i] = mDevice.createSemaphore(semaphoreCreateInfo);
		mInFlightFences[i] = mDevice.createFence(fenceCreateInfo);
	}
}

void Application::recordCommandBuffer(vk::CommandBuffer cmdBuffer, uint32_t imgIndex)
{
	auto beginInfo = vk::CommandBufferBeginInfo()
		.setFlags(vk::CommandBufferUsageFlagBits{})
		.setPInheritanceInfo(nullptr);

	cmdBuffer.begin(beginInfo);

	auto clearValue = vk::ClearValue()
		.setColor(vk::ClearColorValue().setFloat32({ 0.0f, 0.0f, 0.0f, 1.0f }));

	auto renderPassBeginInfo = vk::RenderPassBeginInfo()
		.setRenderPass(mRenderPass)
		.setFramebuffer(mSwapchainFramebuffers[imgIndex])
		.setRenderArea({ { 0, 0 }, mSwapchainExtent })
		.setClearValues(clearValue);

	cmdBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

	cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mGraphicsPipeline);

	cmdBuffer.bindVertexBuffers(0, mVertexBuffer, vk::DeviceSize(0));
	cmdBuffer.bindIndexBuffer(mIndexBuffer, 0, vk::IndexType::eUint32);

	cmdBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, mSwapchainExtent.width, mSwapchainExtent.height, 0.0f, 1.0f));
	cmdBuffer.setScissor(0, vk::Rect2D({ 0, 0 }, mSwapchainExtent));
	//cmdBuffer.draw(VertexData.size(), 1, 0, 0);
	cmdBuffer.drawIndexed(IndexData.size(), 1, 0, 0, 0);

	cmdBuffer.endRenderPass();
	cmdBuffer.end();
}

void Application::drawFrame()
{
	if (mDevice.waitForFences(mInFlightFences[mCurFrameIdx], true, UINT64_MAX) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to wait for any fences");

	auto [acquireRes, imgIndex] = mDevice.acquireNextImageKHR(mSwapchain, UINT64_MAX,
		mImageAvailableSemaphores[mCurFrameIdx]);
	if (acquireRes == vk::Result::eErrorOutOfDateKHR)
	{
		recreateSwapchain();
		return;
	}
	else if (acquireRes != vk::Result::eSuccess && acquireRes != vk::Result::eSuboptimalKHR)
		throw std::runtime_error("Failed to acquire image from swapchain");

	mDevice.resetFences(mInFlightFences[mCurFrameIdx]);

	mCommandBuffers[mCurFrameIdx].reset();
	recordCommandBuffer(mCommandBuffers[mCurFrameIdx], imgIndex);

	vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	auto submitInfo = vk::SubmitInfo()
		.setCommandBuffers(mCommandBuffers[mCurFrameIdx])
		.setWaitSemaphores(mImageAvailableSemaphores[mCurFrameIdx])
		.setWaitDstStageMask(waitStage)
		.setSignalSemaphores(mRenderFinishedSemaphores[mCurFrameIdx]);

	mGraphicsQueue.submit(submitInfo, mInFlightFences[mCurFrameIdx]);

	auto presentInfo = vk::PresentInfoKHR()
		.setWaitSemaphores(mRenderFinishedSemaphores[mCurFrameIdx])
		.setSwapchains(mSwapchain)
		.setImageIndices(imgIndex);

	try
	{
		auto presentRes = mPresentQueue.presentKHR(presentInfo);
	}
	catch (const vk::SystemError& e)
	{
		recreateSwapchain();
	}
	if (mShouldResetSwapchain)
		recreateSwapchain();

	mCurFrameIdx = (mCurFrameIdx + 1) % NumFramesConcurrent;
	/*auto presentRes = mPresentQueue.presentKHR(presentInfo);

	if (presentRes == vk::Result::eErrorOutOfDateKHR ||
		presentRes == vk::Result::eSuboptimalKHR || mShouldResetSwapchain)
	{
		mShouldResetSwapchain = false;
		recreateSwapchain();
	}
	else if (presentRes != vk::Result::eSuccess)
		throw std::runtime_error("Unable to present");*/
}

void Application::recreateSwapchain()
{
	int width, height;
	glfwGetFramebufferSize(mMainWindow, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(mMainWindow, &width, &height);
		glfwWaitEvents();
	}

	mDevice.waitIdle();
	cleanupSwapchain();

	createSwapchain();
	createSwapchainImageViews();
	createFramebuffers();
}

void Application::cleanupSwapchain()
{
	for (auto& framebuffer : mSwapchainFramebuffers)
		mDevice.destroyFramebuffer(framebuffer);
	for (auto& imageView : mSwapchainImageViews)
		mDevice.destroyImageView(imageView);
	mDevice.destroySwapchainKHR(mSwapchain);
}

void Application::cleanupVulkan()
{
	cleanupSwapchain();

	mDevice.destroyBuffer(mVertexBuffer);
	mDevice.freeMemory(mVertexBufferMemory);
	mDevice.destroyBuffer(mIndexBuffer);
	mDevice.freeMemory(mIndexBufferMemory);

	mDevice.destroyPipeline(mGraphicsPipeline);
	mDevice.destroyPipelineLayout(mPipelineLayout);

	mDevice.destroyRenderPass(mRenderPass);

	for (auto& fence : mInFlightFences)
		mDevice.destroyFence(fence);
	for (auto& sema : mImageAvailableSemaphores)
		mDevice.destroySemaphore(sema);
	for (auto& sema : mRenderFinishedSemaphores)
		mDevice.destroySemaphore(sema);

	mDevice.destroyCommandPool(mCommandPool);

	mShaderManager.destroyShaderModules();

	mDevice.destroy();

	if (EnableValidationLayer)
		//mVkInstance.destroyDebugUtilsMessengerEXT(mVkDebugMessenger);
		mZvkExt.destroyDebugUtilsMessenger(mDebugMessenger);

	mInstance.destroySurfaceKHR(mSurface);
	mInstance.destroy();
}

void Application::exec()
{
	initWindow();
	initVulkan();
	glfwShowWindow(mMainWindow);
	
	while (!glfwWindowShouldClose(mMainWindow))
	{
		glfwPollEvents();
		drawFrame();
	}
	mDevice.waitIdle();

	cleanupVulkan();
	glfwDestroyWindow(mMainWindow);
	glfwTerminate();
}
