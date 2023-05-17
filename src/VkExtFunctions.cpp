#include "VkExtFunctions.h"

NAMESPACE_BEGIN(zvk)

ExtFunctions::ExtFunctions(vk::Instance instance) :
	mInstance(instance)
{
	Log::bracketLine<0>("vk::Instance loading Vulkan ext functions");

	vkCreateDebugUtilsMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)
		vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (vkCreateDebugUtilsMessenger)
		Log::bracketLine<1>("vkCreateDebugUtilsMessengerEXT");

	vkDestroyDebugUtilsMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT)
		vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (vkDestroyDebugUtilsMessenger)
		Log::bracketLine<1>("vkDestroyDebugUtilsMessengerEXT");
}

vk::DebugUtilsMessengerEXT ExtFunctions::createDebugUtilsMessenger(const vk::DebugUtilsMessengerCreateInfoEXT& createInfo) {
	VkDebugUtilsMessengerEXT messenger;
	vkCreateDebugUtilsMessenger(mInstance,
		reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT*>(&createInfo),
		nullptr, &messenger);
	return vk::DebugUtilsMessengerEXT(messenger);
}

void ExtFunctions::destroyDebugUtilsMessenger(vk::DebugUtilsMessengerEXT messenger) {
	vkDestroyDebugUtilsMessenger(mInstance, messenger, nullptr);
}

NAMESPACE_END(zvk)