#pragma once

#include <vulkan/vulkan.hpp>
#include "util/Error.h"
#include "util/NamespaceDecl.h"

NAMESPACE_BEGIN(zvk)

class ExtFunctions {
public:
	ExtFunctions() = default;
	ExtFunctions(vk::Instance instance);

	vk::DebugUtilsMessengerEXT createDebugUtilsMessengerEXT(const vk::DebugUtilsMessengerCreateInfoEXT &createInfo) const;

	void destroyDebugUtilsMessenger(vk::DebugUtilsMessengerEXT messenger) const;

	vk::AccelerationStructureBuildSizesInfoKHR getAccelerationStructureBuildSizesKHR(
		vk::Device device,
		vk::AccelerationStructureBuildTypeKHR buildType,
		const vk::AccelerationStructureBuildGeometryInfoKHR& buildInfo,
		vk::ArrayProxy<const uint32_t> const& maxPrimitiveCounts) const;

	vk::AccelerationStructureKHR createAccelerationStructureKHR(
		vk::Device device,
		const vk::AccelerationStructureCreateInfoKHR& createInfo) const;

	vk::DeviceAddress getAccelerationStructureDeviceAddressKHR(
		vk::Device device,
		const vk::AccelerationStructureDeviceAddressInfoKHR& info) const;

	void cmdBuildAccelerationStructuresKHR(
		vk::CommandBuffer commandBuffer,
		vk::ArrayProxy<const vk::AccelerationStructureBuildGeometryInfoKHR> const& infos,
		vk::ArrayProxy<const vk::AccelerationStructureBuildRangeInfoKHR* const> const& pBuildRangeInfos) const;

	void destroyAccelerationStructureKHR(
		vk::Device device,
		vk::AccelerationStructureKHR accelerationStructure) const;

private:
	vk::Instance mInstance;
	PFN_vkCreateDebugUtilsMessengerEXT fpCreateDebugUtilsMessengerEXT = nullptr;
	PFN_vkDestroyDebugUtilsMessengerEXT fpDestroyDebugUtilsMessengerEXT = nullptr;

	PFN_vkGetAccelerationStructureBuildSizesKHR fpGetAccelerationStructureBuildSizesKHR = nullptr;
	PFN_vkCreateAccelerationStructureKHR fpCreateAccelerationStructureKHR = nullptr;
	PFN_vkDestroyAccelerationStructureKHR fpDestroyAccelerationStructureKHR = nullptr;
	PFN_vkGetAccelerationStructureDeviceAddressKHR fpGetAccelerationStructureDeviceAddressKHR = nullptr;
	PFN_vkCmdBuildAccelerationStructuresKHR fpCmdBuildAccelerationStructuresKHR = nullptr;
};

NAMESPACE_END(zvk)