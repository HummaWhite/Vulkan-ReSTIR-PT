#pragma once

#include <vulkan/vulkan.hpp>
#include "util/Error.h"

class ZvkExt {
public:
	ZvkExt() = default;
	ZvkExt(vk::Instance instance);

	vk::DebugUtilsMessengerEXT createDebugUtilsMessenger(
		const vk::DebugUtilsMessengerCreateInfoEXT &createInfo);
	void destroyDebugUtilsMessenger(vk::DebugUtilsMessengerEXT messenger);

private:
	vk::Instance mInstance;
	PFN_vkCreateDebugUtilsMessengerEXT zvkCreateDebugUtilsMessenger = nullptr;
	PFN_vkDestroyDebugUtilsMessengerEXT zvkDestroyDebugUtilsMessenger = nullptr;
};