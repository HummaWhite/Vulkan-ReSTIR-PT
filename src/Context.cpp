#include "Context.h"
#include "VkDebugLayers.h"

#include <set>
#include <map>

NAMESPACE_BEGIN(zvk)

Context::Context(const Instance& instance, const std::vector<const char*>& extensions) {
	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;

	auto [general, compute, transfer] = instance.queueFamilies();

	if (!general) {
		throw std::runtime_error("no queue family usable");
	}
	std::map<uint32_t, size_t> maxQueueNums = { { general->index, general->count } };

	if (compute) {
		maxQueueNums.insert({ compute->index, compute->count });
	}
	if (transfer) {
		maxQueueNums.insert({ transfer->index, transfer->count });
	}

	uint32_t nGeneral = 0;
	uint32_t generalFamily = general->index;
	uint32_t generalIdx = nGeneral++;

	uint32_t presentFamily = general->index;
	uint32_t presentIdx = std::min(nGeneral++, general->count - 1);

	uint32_t computeFamily = compute ? compute->index : general->index;
	uint32_t computeIdx = compute ? 0 : std::min(nGeneral++, general->count - 1);

	uint32_t transferFamily = transfer ? transfer->index : general->index;
	uint32_t transferIdx = transfer ? 0 : std::min(nGeneral++, general->count - 1);

	std::map<uint32_t, std::set<uint32_t>> uniqueFamilies;
	uniqueFamilies[generalFamily] = uniqueFamilies[computeFamily] = uniqueFamilies[transferFamily] = {};
	uniqueFamilies[generalFamily].insert(generalIdx);
	uniqueFamilies[presentFamily].insert(presentIdx);
	uniqueFamilies[computeFamily].insert(computeIdx);
	uniqueFamilies[transferFamily].insert(transferFamily);

	float priorities[] = { 1.f, 1.f, 1.f, 1.f };

	for (const auto& [family, queues] : uniqueFamilies) {

		auto createInfo = vk::DeviceQueueCreateInfo()
			.setQueueFamilyIndex(family)
			.setQueuePriorities(priorities)
			.setQueueCount(std::min(queues.size(), maxQueueNums[family]));

		queueCreateInfos.push_back(createInfo);
	}
	auto deviceFeatures = vk::PhysicalDeviceFeatures();

	auto deviceCreateInfo = vk::DeviceCreateInfo()
		.setQueueCreateInfos(queueCreateInfos)
		.setPEnabledFeatures(&deviceFeatures)
		.setPEnabledLayerNames(ValidationLayers)
		.setPEnabledExtensionNames(extensions);

	device = instance.physicalDevice().createDevice(deviceCreateInfo);
	memProperties = instance.physicalDevice().getMemoryProperties();

	queues[QueueIdx::GeneralUse] = Queue(device, generalFamily, generalIdx);
	queues[QueueIdx::Present] = Queue(device, presentFamily, presentIdx);
	queues[QueueIdx::AsyncCompute] = Queue(device, computeFamily, computeIdx);
	queues[QueueIdx::AsyncTransfer] = Queue(device, transferFamily, transferIdx);

	createCmdPools();
}

void Context::destroy() {
	std::set<vk::CommandPool> pools(cmdPools.array().begin(), cmdPools.array().end());
	for (auto& pool : pools) {
		device.destroyCommandPool(pool);
	}
	device.destroy();
}

void Context::createCmdPools() {
	std::map<uint32_t, vk::CommandPool> pools;
	
	for (size_t i = 0; i < queues.array().size(); i++) {
		uint32_t idx = queues.array()[i].familyIdx;

		if (pools.find(idx) == pools.end()) {
			auto createInfo = vk::CommandPoolCreateInfo()
				.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
				.setQueueFamilyIndex(idx);
			pools[idx] = device.createCommandPool(createInfo);
		}
		cmdPools.array()[i] = pools[idx];
	}
}

NAMESPACE_END(zvk)
