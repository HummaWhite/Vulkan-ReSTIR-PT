#pragma once

#include <vulkan/vulkan.hpp>
#include "util/Error.h"
#include "util/NamespaceDecl.h"

NAMESPACE_BEGIN(zvk)

class ExtFunctions {
public:
	ExtFunctions() = default;
	ExtFunctions(vk::Instance instance);

	vk::DebugUtilsMessengerEXT createDebugUtilsMessenger(
		const vk::DebugUtilsMessengerCreateInfoEXT &createInfo);
	void destroyDebugUtilsMessenger(vk::DebugUtilsMessengerEXT messenger);

private:
	vk::Instance mInstance;
	PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessenger = nullptr;
	PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessenger = nullptr;
};

NAMESPACE_END(zvk)