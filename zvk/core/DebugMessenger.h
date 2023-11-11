#pragma once

#include <vulkan/vulkan.hpp>

#include <util/NamespaceDecl.h>

NAMESPACE_BEGIN(zvk)

const char* getObjectTypeString(VkObjectType type);

VKAPI_ATTR VkBool32 VKAPI_CALL debugMessengerCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData);

NAMESPACE_END(zvk)