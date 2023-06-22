#pragma once

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <optional>
#include <vector>

#include "VkExtFunctions.h"
#include "util/NamespaceDecl.h"

NAMESPACE_BEGIN(zvk)

struct QueueFamily {
	uint32_t index;
	uint32_t count;
};

struct QueueFamilies {
	std::optional<QueueFamily> generalUse;
	std::optional<QueueFamily> asyncCompute;
	std::optional<QueueFamily> asyncTransfer;
};

class Instance {
public:
	Instance(
		const vk::ApplicationInfo& appInfo, GLFWwindow* window,
		const std::vector<const char*>& extensions, const void* featureChain = nullptr);

	~Instance() { destroy(); }
	void destroy();

	vk::Instance instance() const { return mInstance; }
	vk::PhysicalDevice physicalDevice() const { return mPhysicalDevice; }
	vk::SurfaceKHR surface() const { return mSurface; }
	QueueFamilies queueFamilies() const { return mQueueFamilies; }
	const vk::PhysicalDeviceFeatures& deviceFeatures() const { return mDeviceFeatures; }
	const vk::PhysicalDeviceProperties& deviceProperties() const { return mDeviceProperties; }
	const vk::PhysicalDeviceMemoryProperties& memProperties() const { return mMemProperties; }
	const ExtFunctions& extFunctions() const { return mExtFunctions; }

private:
	void queryExtensionsAndLayers();
	void setupDebugMessenger();
	void createSurface(GLFWwindow* window);
	QueueFamilies getQueueFamilies(vk::PhysicalDevice device);
	bool hasDeviceExtensions(vk::PhysicalDevice device, const std::vector<const char*>& extensions);
	void selectPhysicalDevice(const std::vector<const char*>& extensions);

private:
	vk::Instance mInstance;
	vk::PhysicalDevice mPhysicalDevice;
	vk::SurfaceKHR mSurface;
	vk::PhysicalDeviceFeatures mDeviceFeatures;
	vk::PhysicalDeviceProperties mDeviceProperties;
	vk::PhysicalDeviceMemoryProperties mMemProperties;

	QueueFamilies mQueueFamilies;

	ExtFunctions mExtFunctions;
	vk::DebugUtilsMessengerEXT mDebugMessenger;

	std::vector<char*> mRequiredVkExtensions;
	std::vector<char*> mRequiredVkLayers;
};

NAMESPACE_END(zvk)