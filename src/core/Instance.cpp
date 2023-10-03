#include "Instance.h"
#include "VkDebugLayers.h"
#include "ExtFunctions.h"
#include "DebugUtils.h"

#include "util/EnumBitField.h"
#include "util/Error.h"

#include <set>

NAMESPACE_BEGIN(zvk)

static VKAPI_ATTR VkBool32 VKAPI_CALL zvkDebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData
) {
	Log::newLine();
	Log::line<0>(std::string(pCallbackData->pMessage));
	return VK_FALSE;
}

vk::DebugUtilsMessengerCreateInfoEXT zvkNormalDebugCreateInfo() {
	using Severity = vk::DebugUtilsMessageSeverityFlagBitsEXT;
	using MsgType = vk::DebugUtilsMessageTypeFlagBitsEXT;

	auto createInfo = vk::DebugUtilsMessengerCreateInfoEXT()
		.setMessageSeverity(Severity::eWarning | Severity::eError)
		.setMessageType(MsgType::eGeneral | MsgType::eValidation | MsgType::ePerformance)
		.setPfnUserCallback(zvkDebugCallback);

	return createInfo;
}

Instance::Instance(
	const vk::ApplicationInfo& appInfo, GLFWwindow* window,
	const std::vector<const char*>& extensions, const void* featureChain
) {
	queryExtensionsAndLayers();
	auto debugInfo = zvkNormalDebugCreateInfo();

	if (EnableValidationLayer) {
		debugInfo.setPNext(featureChain);
	}
	auto instanceInfo = vk::InstanceCreateInfo()
		.setPApplicationInfo(&appInfo)
		.setPEnabledExtensionNames(mRequiredVkExtensions)
		.setPEnabledLayerNames(ValidationLayers)
		.setPNext(EnableValidationLayer ? &debugInfo : featureChain);

	mInstance = vk::createInstance(instanceInfo);
	ExtFunctions::load(mInstance);

	setupDebugMessenger();
	createSurface(window);
	selectPhysicalDevice(extensions);
	queryPhysicalDeviceProperties();
}

void Instance::destroy() {
	if (mDebugMessenger) {
		//mVkInstance.destroyDebugUtilsMessengerEXT(mVkDebugMessenger);
		ExtFunctions::destroyDebugUtilsMessenger(mInstance, mDebugMessenger);
	}
	mInstance.destroySurfaceKHR(mSurface);
	mInstance.destroy();
}

void Instance::queryExtensionsAndLayers() {
	auto extensionProps = vk::enumerateInstanceExtensionProperties();
	Log::line<0>("Instance extensions");

	for (const auto& i : extensionProps) {
		Log::line<1>(i.extensionName);
	}

	uint32_t nExtensions;
	auto extensions = glfwGetRequiredInstanceExtensions(&nExtensions);
	mRequiredVkExtensions.resize(nExtensions);
	memcpy(mRequiredVkExtensions.data(), extensions, sizeof(char*) * nExtensions);

	if (EnableValidationLayer) {
		mRequiredVkExtensions.push_back((char*)VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	Log::newLine();

	Log::line<0>("Instance layers");
	auto layerProps = vk::enumerateInstanceLayerProperties();

	for (const auto& i : layerProps) {
		Log::line<1>(i.layerName);
	}

	if (!EnableValidationLayer) {
		return;
	}
	for (const auto& layerName : ValidationLayers) {
		bool found = false;
		for (const auto& prop : layerProps) {
			if (!std::strcmp(prop.layerName, layerName)) {
				found = true;
				break;
			}
		}
		if (!found) {
			throw std::runtime_error("Required layer: " + std::string(layerName) + " not supported");
		}
	}
	Log::newLine();
}

void Instance::setupDebugMessenger() {
	if (!EnableValidationLayer) {
		return;
	}
	auto createInfo = zvkNormalDebugCreateInfo();
	//mVkDebugMessenger = mVkInstance.createDebugUtilsMessengerEXT(createInfo);
	mDebugMessenger = ExtFunctions::createDebugUtilsMessengerEXT(mInstance, createInfo);
}

void Instance::createSurface(GLFWwindow* window) {
	if (glfwCreateWindowSurface(mInstance, window, nullptr,
		reinterpret_cast<VkSurfaceKHR*>(&mSurface)) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create surface");
	}
	/*auto createInfo = vk::Win32SurfaceCreateInfoKHR()
		.setHwnd(glfwGetWin32Window(mMainWindow))
		.setHinstance(GetModuleHandle(nullptr));
	mVkSurface = mVkInstance.createWin32SurfaceKHR(createInfo);*/
}

QueueFamilies Instance::getQueueFamilies(vk::PhysicalDevice device) {
	auto queueFamilyProps = device.getQueueFamilyProperties();
	Log::line<2>("Queue families");

	QueueFamilies families;

	for (uint32_t i = 0; i < queueFamilyProps.size(); i++) {
		std::string queueInfo = "Queue count = " + std::to_string(queueFamilyProps[i].queueCount);
		auto flags = queueFamilyProps[i].queueFlags;
		auto count = queueFamilyProps[i].queueCount;

		if (hasFlagBit(flags, vk::QueueFlagBits::eGraphics) &&
			hasFlagBit(flags, vk::QueueFlagBits::eCompute) &&
			device.getSurfaceSupportKHR(i, mSurface)
			) {
			families.generalUse = QueueFamily{ i, count };
			queueInfo += " | Graphics | Compute | Transfer | Present";
		}

		if (hasFlagBit(flags, vk::QueueFlagBits::eCompute) &&
			!hasFlagBit(flags, vk::QueueFlagBits::eGraphics)
			) {
			families.asyncCompute = QueueFamily{ i, count };
			queueInfo += " | Async Compute";
		}

		if (hasFlagBit(flags, vk::QueueFlagBits::eTransfer) &&
			!hasFlagBit(flags, vk::QueueFlagBits::eGraphics) &&
			!hasFlagBit(flags, vk::QueueFlagBits::eCompute)
			) {
			families.asyncTransfer = QueueFamily{ i, count };
			queueInfo += " | Async Transfer";
		}
		Log::line<3>(queueInfo);
	};
	return families;
}

bool Instance::hasDeviceExtensions(vk::PhysicalDevice device, const std::vector<const char*>& extensions) {
	auto extensionProps = device.enumerateDeviceExtensionProperties();
	std::set<std::string> requiredExtensions(extensions.begin(), extensions.end());

	for (const auto& e : extensionProps) {
		requiredExtensions.erase(e.extensionName);
	}
	return requiredExtensions.empty();
}

void Instance::selectPhysicalDevice(const std::vector<const char*>& extensions) {
	auto physicalDevices = mInstance.enumeratePhysicalDevices();

	if (physicalDevices.empty()) {
		throw std::runtime_error("No physical device found");
	}
	Log::line<0>(std::to_string(physicalDevices.size()) + " physical device(s) found");

	for (const auto& device : physicalDevices) {
		auto props = device.getProperties();
		auto features = device.getFeatures();

		Log::line<1>("Device " + std::to_string(props.deviceID) + ", " +
			props.deviceName.data() + ", driver = " + std::to_string(props.driverVersion) +
			", API = " + std::to_string(props.apiVersion));

		auto queueFamilies = getQueueFamilies(device);

		if (hasDeviceExtensions(device, extensions) &&
			!device.getSurfaceFormatsKHR(mSurface).empty() &&
			!device.getSurfacePresentModesKHR(mSurface).empty() &&
			queueFamilies.generalUse
		) {
			mPhysicalDevice = device;
			mQueueFamilies = queueFamilies;
		}
	}
	if (!mPhysicalDevice) {
		throw std::runtime_error("No physical device available");
	}
	Log::line<1>("Selected device: " + std::string(mPhysicalDevice.getProperties().deviceName.data()));
	Log::newLine();
}

void Instance::queryPhysicalDeviceProperties() {
	using RTPipelineProperties = vk::PhysicalDeviceRayTracingPipelinePropertiesKHR;
	using ASProperties = vk::PhysicalDeviceAccelerationStructurePropertiesKHR;

	memProperties = mPhysicalDevice.getMemoryProperties();
	deviceProperties = mPhysicalDevice.getProperties();

	vk::PhysicalDeviceProperties2 props2;
	props2.pNext = &rayTracingPipelineProperties;
	rayTracingPipelineProperties.pNext = &accelerationStructureProperties;

	mPhysicalDevice.getProperties2(&props2);
	deviceProperties = props2.properties;
}

NAMESPACE_END(zvk)
