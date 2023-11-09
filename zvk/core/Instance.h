#pragma once

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <optional>
#include <vector>

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
		const std::vector<const char*>& requestedDeviceExtensions,
		const void* featureChain = nullptr);

	~Instance() { destroy(); }
	void destroy();

	vk::Instance instance() const { return mInstance; }
	vk::PhysicalDevice physicalDevice() const { return mPhysicalDevice; }
	vk::SurfaceKHR surface() const { return mSurface; }
	QueueFamilies queueFamilies() const { return mQueueFamilies; }

private:
	void queryExtensionsAndLayers();
	void setupDebugMessenger();
	void createSurface(GLFWwindow* window);
	QueueFamilies getQueueFamilies(vk::PhysicalDevice device);
	bool hasDeviceExtensions(vk::PhysicalDevice device, const std::vector<const char*>& extensions);
	void selectPhysicalDevice(const std::vector<const char*>& extensions);

	void queryPhysicalDeviceProperties();

public:
	vk::PhysicalDeviceFeatures deviceFeatures;
	vk::PhysicalDeviceProperties deviceProperties;
	vk::PhysicalDeviceMemoryProperties memProperties;
	vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties;
	vk::PhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProperties;

private:
	vk::Instance mInstance;
	vk::SurfaceKHR mSurface;
	vk::PhysicalDevice mPhysicalDevice;

	QueueFamilies mQueueFamilies;

	vk::DebugUtilsMessengerEXT mDebugMessenger;

	std::vector<char*> mRequiredVkExtensions;
	std::vector<char*> mRequiredVkLayers;
};

NAMESPACE_END(zvk)