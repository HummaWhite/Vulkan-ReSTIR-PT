#include "ExtFunctions.h"
#include <util/Error.h>

#define ZVK_EXT_FUNCTIONS_LOAD_LOG true

NAMESPACE_BEGIN(zvk::ExtFunctions)

static PFN_vkGetAccelerationStructureBuildSizesKHR fpGetAccelerationStructureBuildSizesKHR = nullptr;
static PFN_vkCreateAccelerationStructureKHR fpCreateAccelerationStructureKHR = nullptr;
static PFN_vkDestroyAccelerationStructureKHR fpDestroyAccelerationStructureKHR = nullptr;
static PFN_vkGetAccelerationStructureDeviceAddressKHR fpGetAccelerationStructureDeviceAddressKHR = nullptr;
static PFN_vkCmdBuildAccelerationStructuresKHR fpCmdBuildAccelerationStructuresKHR = nullptr;
static PFN_vkGetRayTracingShaderGroupHandlesKHR fpGetRayTracingShaderGroupHandlesKHR = nullptr;
static PFN_vkCreateRayTracingPipelinesKHR fpCreateRayTracingPipelinesKHR = nullptr;
static PFN_vkCmdTraceRaysKHR fpCmdTraceRaysKHR = nullptr;

static PFN_vkCreateDebugUtilsMessengerEXT fpCreateDebugUtilsMessengerEXT = nullptr;
static PFN_vkDestroyDebugUtilsMessengerEXT fpDestroyDebugUtilsMessengerEXT = nullptr;
static PFN_vkSetDebugUtilsObjectTagEXT fpSetDebugUtilsObjectTagEXT = nullptr;
static PFN_vkSetDebugUtilsObjectNameEXT fpSetDebugUtilsObjectNameEXT = nullptr;
static PFN_vkCmdBeginDebugUtilsLabelEXT fpCmdBeginDebugUtilsLabelEXT = nullptr;
static PFN_vkCmdEndDebugUtilsLabelEXT fpCmdEndDebugUtilsLabelEXT = nullptr;
static PFN_vkCmdInsertDebugUtilsLabelEXT fpCmdInsertDebugUtilsLabelEXT = nullptr;

template<typename FuncPtr>
void loadFunction(vk::Instance instance, const char* name, FuncPtr& func) {
	func = reinterpret_cast<FuncPtr>(vkGetInstanceProcAddr(instance, name));
#if ZVK_EXT_FUNCTIONS_LOAD_LOG
	if (func) {
		Log::line<1>(name);
	}
#endif
}

void load(vk::Instance instance) {
	Log::line<0>("Loading Vulkan functions");

	loadFunction(instance, "vkGetAccelerationStructureBuildSizesKHR", fpGetAccelerationStructureBuildSizesKHR);
	loadFunction(instance, "vkCreateAccelerationStructureKHR", fpCreateAccelerationStructureKHR);
	loadFunction(instance, "vkGetAccelerationStructureDeviceAddressKHR", fpGetAccelerationStructureDeviceAddressKHR);
	loadFunction(instance, "vkCmdBuildAccelerationStructuresKHR", fpCmdBuildAccelerationStructuresKHR);
	loadFunction(instance, "vkDestroyAccelerationStructureKHR", fpDestroyAccelerationStructureKHR);
	loadFunction(instance, "vkGetRayTracingShaderGroupHandlesKHR", fpGetRayTracingShaderGroupHandlesKHR);
	loadFunction(instance, "vkCreateRayTracingPipelinesKHR", fpCreateRayTracingPipelinesKHR);
	loadFunction(instance, "vkCmdTraceRaysKHR", fpCmdTraceRaysKHR);

	loadFunction(instance, "vkCreateDebugUtilsMessengerEXT", fpCreateDebugUtilsMessengerEXT);
	loadFunction(instance, "vkDestroyDebugUtilsMessengerEXT", fpDestroyDebugUtilsMessengerEXT);
	loadFunction(instance, "vkSetDebugUtilsObjectTagEXT", fpSetDebugUtilsObjectTagEXT);
	loadFunction(instance, "vkSetDebugUtilsObjectNameEXT", fpSetDebugUtilsObjectNameEXT);
	loadFunction(instance, "vkCmdBeginDebugUtilsLabelEXT", fpCmdBeginDebugUtilsLabelEXT);
	loadFunction(instance, "vkCmdEndDebugUtilsLabelEXT", fpCmdEndDebugUtilsLabelEXT);
	loadFunction(instance, "vkCmdInsertDebugUtilsLabelEXT", fpCmdInsertDebugUtilsLabelEXT);

	Log::newLine();
}

vk::AccelerationStructureBuildSizesInfoKHR getAccelerationStructureBuildSizesKHR(
	vk::Device device,
	vk::AccelerationStructureBuildTypeKHR buildType,
	const vk::AccelerationStructureBuildGeometryInfoKHR& buildInfo,
	vk::ArrayProxy<const uint32_t> const& maxPrimitiveCounts
) {
	VkAccelerationStructureBuildSizesInfoKHR buildSize {
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
	};

	fpGetAccelerationStructureBuildSizesKHR(
		device,
		static_cast<VkAccelerationStructureBuildTypeKHR>(buildType),
		reinterpret_cast<const VkAccelerationStructureBuildGeometryInfoKHR*>(&buildInfo),
		maxPrimitiveCounts.data(), &buildSize
	);
	return buildSize;
}

vk::AccelerationStructureKHR createAccelerationStructureKHR(
	vk::Device device,
	const vk::AccelerationStructureCreateInfoKHR& createInfo
) {
	VkAccelerationStructureKHR structure;

	auto result = fpCreateAccelerationStructureKHR(
		device,
		reinterpret_cast<const VkAccelerationStructureCreateInfoKHR*>(&createInfo),
		nullptr,
		&structure
	);

	if (result != VK_SUCCESS) {
		vk::detail::throwResultException(vk::Result(result), "zvk::VkExt vkCreateAccelerationStructureKHR failed");
	}
	return structure;
}

vk::DeviceAddress getAccelerationStructureDeviceAddressKHR(
	vk::Device device,
	const vk::AccelerationStructureDeviceAddressInfoKHR& info
) {
	return fpGetAccelerationStructureDeviceAddressKHR(
		device,
		reinterpret_cast<const VkAccelerationStructureDeviceAddressInfoKHR*>(&info)
	);
}

void cmdBuildAccelerationStructuresKHR(
	vk::CommandBuffer commandBuffer,
	vk::ArrayProxy<const vk::AccelerationStructureBuildGeometryInfoKHR> const& infos,
	vk::ArrayProxy<const vk::AccelerationStructureBuildRangeInfoKHR* const> const& pBuildRangeInfos
) {
	fpCmdBuildAccelerationStructuresKHR(
		commandBuffer,
		infos.size(),
		reinterpret_cast<const VkAccelerationStructureBuildGeometryInfoKHR*>(infos.data()),
		reinterpret_cast<const VkAccelerationStructureBuildRangeInfoKHR* const*>(pBuildRangeInfos.data())
	);
}

std::vector<uint8_t> getRayTracingShaderGroupHandlesKHR(
	vk::Device device,
	vk::Pipeline pipeline,
	uint32_t firstGroup,
	uint32_t groupCount,
	size_t dataSize
) {
	std::vector<uint8_t> handles(dataSize);

	auto result = fpGetRayTracingShaderGroupHandlesKHR(
		device,
		pipeline,
		firstGroup,
		groupCount,
		dataSize,
		handles.data()
	);

	if (result != VK_SUCCESS) {
		vk::detail::throwResultException(vk::Result(result), "zvk::VkExt vkGetRayTracingShaderGroupHandlesKHR failed");
	}
	return handles;
}

vk::ResultValue<vk::Pipeline> createRayTracingPipelineKHR(
	vk::Device device,
	vk::DeferredOperationKHR deferredOperation,
	vk::PipelineCache pipelineCache,
	const vk::RayTracingPipelineCreateInfoKHR& createInfo
) {
	vk::Pipeline pipeline;

	auto result = fpCreateRayTracingPipelinesKHR(
		device,
		deferredOperation,
		pipelineCache,
		1,
		reinterpret_cast<const VkRayTracingPipelineCreateInfoKHR*>(&createInfo),
		nullptr,
		reinterpret_cast<VkPipeline*>(&pipeline)
	);

	return { vk::Result(result), pipeline };
}

void cmdTraceRaysKHR(
	vk::CommandBuffer commandBuffer,
	const vk::StridedDeviceAddressRegionKHR& raygenShaderBindingTable,
	const vk::StridedDeviceAddressRegionKHR& missShaderBindingTable,
	const vk::StridedDeviceAddressRegionKHR& hitShaderBindingTable,
	const vk::StridedDeviceAddressRegionKHR& callableShaderBindingTable,
	uint32_t width,
	uint32_t height,
	uint32_t depth
) {
	fpCmdTraceRaysKHR(
		commandBuffer,
		reinterpret_cast<const VkStridedDeviceAddressRegionKHR*>(&raygenShaderBindingTable),
		reinterpret_cast<const VkStridedDeviceAddressRegionKHR*>(&missShaderBindingTable),
		reinterpret_cast<const VkStridedDeviceAddressRegionKHR*>(&hitShaderBindingTable),
		reinterpret_cast<const VkStridedDeviceAddressRegionKHR*>(&callableShaderBindingTable),
		width,
		height,
		depth
	);
}

vk::DebugUtilsMessengerEXT createDebugUtilsMessengerEXT(
	vk::Instance instance,
	const vk::DebugUtilsMessengerCreateInfoEXT& createInfo
) {
	VkDebugUtilsMessengerEXT messenger;

	auto result = fpCreateDebugUtilsMessengerEXT(
		instance,
		reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT*>(&createInfo),
		nullptr, &messenger
	);

	if (result != VK_SUCCESS) {
		vk::detail::throwResultException(vk::Result(result), "zvk::VkExt vkCreateDebugUtilsMessengerEXT failed");
	}
	return vk::DebugUtilsMessengerEXT(messenger);
}

void destroyDebugUtilsMessenger(
	vk::Instance instance,
	vk::DebugUtilsMessengerEXT messenger
) {
	fpDestroyDebugUtilsMessengerEXT(
		instance,
		messenger,
		nullptr
	);
}

void destroyAccelerationStructureKHR(
	vk::Device device,
	vk::AccelerationStructureKHR accelerationStructure
) {
	fpDestroyAccelerationStructureKHR(
		device,
		accelerationStructure,
		nullptr
	);
}

void setDebugUtilsObjectTagEXT(
	vk::Device device,
	const vk::DebugUtilsObjectTagInfoEXT& tagInfo
) {
	auto result = fpSetDebugUtilsObjectTagEXT(
		device,
		reinterpret_cast<const VkDebugUtilsObjectTagInfoEXT*>(&tagInfo)
	);

	if (result != VK_SUCCESS) {
		vk::detail::throwResultException(vk::Result(result), "zvk::VkExt vkDebugMarkerSetObjectTagEXT failed");
	}
}

void setDebugUtilsObjectNameEXT(
	vk::Device device,
	const vk::DebugUtilsObjectNameInfoEXT& nameInfo
) {
	auto result = fpSetDebugUtilsObjectNameEXT(
		device,
		reinterpret_cast<const VkDebugUtilsObjectNameInfoEXT*>(&nameInfo)
	);

	if (result != VK_SUCCESS) {
		vk::detail::throwResultException(vk::Result(result), "zvk::VkExt vkDebugMarkerSetObjectNameEXT failed");
	}
}

void cmdBeginDebugUtilsLabelEXT(
	vk::CommandBuffer commandBuffer,
	const vk::DebugUtilsLabelEXT& label
) {
	fpCmdBeginDebugUtilsLabelEXT(
		commandBuffer,
		reinterpret_cast<const VkDebugUtilsLabelEXT*>(&label)
	);
}

void cmdEndDebugUtilsLabelEXT(
	vk::CommandBuffer commandBuffer
) {
	fpCmdEndDebugUtilsLabelEXT(
		commandBuffer
	);
}

void cmdInsertDebugUtilsLabelEXT(
	vk::CommandBuffer commandBuffer,
	const vk::DebugUtilsLabelEXT& label
) {
	fpCmdInsertDebugUtilsLabelEXT(
		commandBuffer,
		reinterpret_cast<const VkDebugUtilsLabelEXT*>(&label)
	);
}

NAMESPACE_END(zvk::ExtFunctions)