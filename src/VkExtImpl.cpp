#include "VkExtImpl.h"

ZvkExt::ZvkExt(vk::Instance instance) :
	mInstance(instance)
{
	Log::bracketLine<0>("vk::Instance loading Vulkan ext functions");

	zvkCreateDebugUtilsMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)
		vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (zvkCreateDebugUtilsMessenger)
		Log::bracketLine<1>("vkCreateDebugUtilsMessengerEXT");

	zvkDestroyDebugUtilsMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT)
		vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (zvkDestroyDebugUtilsMessenger)
		Log::bracketLine<1>("vkDestroyDebugUtilsMessengerEXT");
}

vk::DebugUtilsMessengerEXT ZvkExt::createDebugUtilsMessenger(const vk::DebugUtilsMessengerCreateInfoEXT& createInfo) {
	VkDebugUtilsMessengerEXT messenger;
	zvkCreateDebugUtilsMessenger(mInstance,
		reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT*>(&createInfo),
		nullptr, &messenger);
	return vk::DebugUtilsMessengerEXT(messenger);
}

void ZvkExt::destroyDebugUtilsMessenger(vk::DebugUtilsMessengerEXT messenger) {
	zvkDestroyDebugUtilsMessenger(mInstance, messenger, nullptr);
}
