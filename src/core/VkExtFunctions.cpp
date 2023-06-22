#include "VkExtFunctions.h"

NAMESPACE_BEGIN(zvk)

#define ZVK_EXT_FUNCTIONS_LOAD_LOG true

template<typename FuncPtr>
void loadFunction(vk::Instance instance, const char* name, FuncPtr& func) {
	func = reinterpret_cast<FuncPtr>(vkGetInstanceProcAddr(instance, name));
#if ZVK_EXT_FUNCTIONS_LOAD_LOG
	if (func) {
		Log::bracketLine<1>(name);
	}
#endif
}

ExtFunctions::ExtFunctions(vk::Instance instance) :
	mInstance(instance)
{
	Log::bracketLine<0>("vk::Instance loading Vulkan ext functions");

	loadFunction(instance, "vkCreateDebugUtilsMessengerEXT", fpCreateDebugUtilsMessengerEXT);
	loadFunction(instance, "vkDestroyDebugUtilsMessengerEXT", fpDestroyDebugUtilsMessengerEXT);
	loadFunction(instance, "vkGetAccelerationStructureBuildSizesKHR", fpGetAccelerationStructureBuildSizesKHR);
	loadFunction(instance, "vkCreateAccelerationStructureKHR", fpCreateAccelerationStructureKHR);
	loadFunction(instance, "vkGetAccelerationStructureDeviceAddressKHR", fpGetAccelerationStructureDeviceAddressKHR);
	loadFunction(instance, "vkCmdBuildAccelerationStructuresKHR", fpCmdBuildAccelerationStructuresKHR);
	loadFunction(instance, "vkDestroyAccelerationStructureKHR", fpDestroyAccelerationStructureKHR);
}

vk::DebugUtilsMessengerEXT ExtFunctions::createDebugUtilsMessengerEXT(const vk::DebugUtilsMessengerCreateInfoEXT& createInfo) const {
	VkDebugUtilsMessengerEXT messenger;

	fpCreateDebugUtilsMessengerEXT(
		mInstance,
		reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT*>(&createInfo),
		nullptr, &messenger);
	return vk::DebugUtilsMessengerEXT(messenger);
}

void ExtFunctions::destroyDebugUtilsMessenger(vk::DebugUtilsMessengerEXT messenger) const {
	fpDestroyDebugUtilsMessengerEXT(mInstance, messenger, nullptr);
}

vk::AccelerationStructureBuildSizesInfoKHR ExtFunctions::getAccelerationStructureBuildSizesKHR(
	vk::Device device,
	vk::AccelerationStructureBuildTypeKHR buildType,
	const vk::AccelerationStructureBuildGeometryInfoKHR& buildInfo,
	vk::ArrayProxy<const uint32_t> const& maxPrimitiveCounts
) const {
	VkAccelerationStructureBuildSizesInfoKHR buildSize;

	fpGetAccelerationStructureBuildSizesKHR(
		device,
		static_cast<VkAccelerationStructureBuildTypeKHR>(buildType),
		reinterpret_cast<const VkAccelerationStructureBuildGeometryInfoKHR*>(&buildInfo),
		maxPrimitiveCounts.data(), &buildSize
	);
	return buildSize;
}

vk::AccelerationStructureKHR ExtFunctions::createAccelerationStructureKHR(
	vk::Device device,
	const vk::AccelerationStructureCreateInfoKHR& createInfo
) const {
	VkAccelerationStructureKHR structure;

	auto result = fpCreateAccelerationStructureKHR(
		device,
		reinterpret_cast<const VkAccelerationStructureCreateInfoKHR*>(&createInfo),
		nullptr,
		&structure
	);

	if (result != VK_SUCCESS) {
		vk::throwResultException(vk::Result(result), "createAccelerationStructureKHR failed");
	}
	return structure;
}

vk::DeviceAddress ExtFunctions::getAccelerationStructureDeviceAddressKHR(
	vk::Device device,
	const vk::AccelerationStructureDeviceAddressInfoKHR& info
) const {
	return fpGetAccelerationStructureDeviceAddressKHR(
		device,
		reinterpret_cast<const VkAccelerationStructureDeviceAddressInfoKHR*>(&info)
	);
}

void ExtFunctions::cmdBuildAccelerationStructuresKHR(
	vk::CommandBuffer commandBuffer,
	vk::ArrayProxy<const vk::AccelerationStructureBuildGeometryInfoKHR> const& infos,
	vk::ArrayProxy<const vk::AccelerationStructureBuildRangeInfoKHR* const> const& pBuildRangeInfos
) const {
	fpCmdBuildAccelerationStructuresKHR(
		commandBuffer,
		infos.size(),
		reinterpret_cast<const VkAccelerationStructureBuildGeometryInfoKHR*>(infos.data()),
		reinterpret_cast<const VkAccelerationStructureBuildRangeInfoKHR* const*>(pBuildRangeInfos.data())
	);
}

void ExtFunctions::destroyAccelerationStructureKHR(
	vk::Device device,
	vk::AccelerationStructureKHR accelerationStructure
) const {
	fpDestroyAccelerationStructureKHR(
		device,
		accelerationStructure,
		nullptr
	);
}

NAMESPACE_END(zvk)