#include "Context.h"
#include "VkDebugLayers.h"

#include <set>

NAMESPACE_BEGIN(zvk)

Context::Context(const Instance& instance, const std::vector<const char*>& extensions) {
	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;

	auto [graphics, compute, present, transfer] = instance.queueFamilyIndices();

	std::set<uint32_t> uniqueQueueFamilies = {
		graphics, compute, present, transfer
	};

	for (const auto& i : uniqueQueueFamilies) {
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
		.setPEnabledExtensionNames(extensions);

	device = instance.physicalDevice().createDevice(deviceCreateInfo);

	graphicsQueue = Queue(device, graphics, 0);
	computeQueue = Queue(device, compute, 0);
	presentQueue = Queue(device, present, 0);
	transferQueue = Queue(device, transfer, 0);

	memProperties = instance.physicalDevice().getMemoryProperties();
}

void Context::destroy() {
	device.destroy();
}

NAMESPACE_END(zvk)
