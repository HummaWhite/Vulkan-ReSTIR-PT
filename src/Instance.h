#pragma once

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <optional>
#include <vector>

#include "VkExtFunctions.h"
#include "util/NamespaceDecl.h"

NAMESPACE_BEGIN(zvk)

struct QueueIndices {
	uint32_t graphics;
	uint32_t compute;
	uint32_t present;
	uint32_t transfer;
};

class Instance {
public:
	Instance() = default;
	Instance(const vk::ApplicationInfo& appInfo, GLFWwindow* window, const std::vector<const char*>& extensions);
	void destroy();

	static Instance create(const vk::ApplicationInfo& appInfo, GLFWwindow* window, const std::vector<const char*>& extensions) {
		return Instance(appInfo, window, extensions);
	}

	vk::Instance instance() const { return mInstance; }
	vk::PhysicalDevice physicalDevice() const { return mPhysicalDevice; }
	vk::SurfaceKHR surface() const { return mSurface; }
	QueueIndices queueFamilyIndices() const { return mQueueFamilyIndices; }

private:
	void queryExtensionsAndLayers();
	void setupDebugMessenger();
	void createSurface(GLFWwindow* window);
	std::optional<QueueIndices> getQueueFamilyIndices(vk::PhysicalDevice device);
	bool hasDeviceExtensions(vk::PhysicalDevice device, const std::vector<const char*>& extensions);
	void selectPhysicalDevice(const std::vector<const char*>& extensions);

private:
	vk::Instance mInstance;
	vk::PhysicalDevice mPhysicalDevice;
	vk::SurfaceKHR mSurface;
	vk::PhysicalDeviceMemoryProperties mMemProperties;

	QueueIndices mQueueFamilyIndices;

	ExtFunctions mExtFunctions;
	vk::DebugUtilsMessengerEXT mDebugMessenger;

	std::vector<char*> mRequiredVkExtensions;
	std::vector<char*> mRequiredVkLayers;
};

NAMESPACE_END(zvk)