#include "VkExtFunctions.h"

NAMESPACE_BEGIN(zvk)

#define ZVK_EXT_FUNCTIONS_LOAD_LOG true

template<typename FuncPtr>
void loadFunction(vk::Instance instance, const char* name, FuncPtr& func) {
	func = reinterpret_cast<FuncPtr>(vkGetInstanceProcAddr(instance, name));
#if ZVK_EXT_FUNCTIONS_LOAD_LOG
	if (func) {
		Log::line<1>(name);
	}
#endif
}

ExtFunctions::ExtFunctions(vk::Instance instance) :
	mInstance(instance)
{
	Log::line<0>("Loading Vulkan functions");

	loadFunction(instance, "vkCreateDebugUtilsMessengerEXT", fpCreateDebugUtilsMessengerEXT);
	loadFunction(instance, "vkDestroyDebugUtilsMessengerEXT", fpDestroyDebugUtilsMessengerEXT);
	loadFunction(instance, "vkGetAccelerationStructureBuildSizesKHR", fpGetAccelerationStructureBuildSizesKHR);
	loadFunction(instance, "vkCreateAccelerationStructureKHR", fpCreateAccelerationStructureKHR);
	loadFunction(instance, "vkGetAccelerationStructureDeviceAddressKHR", fpGetAccelerationStructureDeviceAddressKHR);
	loadFunction(instance, "vkCmdBuildAccelerationStructuresKHR", fpCmdBuildAccelerationStructuresKHR);
	loadFunction(instance, "vkDestroyAccelerationStructureKHR", fpDestroyAccelerationStructureKHR);
	loadFunction(instance, "vkGetRayTracingShaderGroupHandlesKHR", fpGetRayTracingShaderGroupHandlesKHR);
	loadFunction(instance, "vkCreateRayTracingPipelinesKHR", fpCreateRayTracingPipelinesKHR);
	loadFunction(instance, "vkCmdTraceRaysKHR", fpCmdTraceRaysKHR);

	Log::newLine();
}

vk::DebugUtilsMessengerEXT ExtFunctions::createDebugUtilsMessengerEXT(const vk::DebugUtilsMessengerCreateInfoEXT& createInfo) const {
	VkDebugUtilsMessengerEXT messenger;

	auto result = fpCreateDebugUtilsMessengerEXT(
		mInstance,
		reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT*>(&createInfo),
		nullptr, &messenger);

	if (result != VK_SUCCESS) {
		vk::throwResultException(vk::Result(result), "zvk::VkExt vkCreateDebugUtilsMessengerEXT failed");
	}
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
		vk::throwResultException(vk::Result(result), "zvk::VkExt vkCreateAccelerationStructureKHR failed");
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

std::vector<uint8_t> ExtFunctions::getRayTracingShaderGroupHandlesKHR(
	vk::Device device,
	vk::Pipeline pipeline,
	uint32_t firstGroup,
	uint32_t groupCount,
	size_t dataSize
) const {
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
		vk::throwResultException(vk::Result(result), "zvk::VkExt vkGetRayTracingShaderGroupHandlesKHR failed");
	}
	return handles;
}

vk::ResultValue<vk::Pipeline> ExtFunctions::createRayTracingPipelineKHR(
	vk::Device device,
	vk::DeferredOperationKHR deferredOperation,
	vk::PipelineCache pipelineCache,
	const vk::RayTracingPipelineCreateInfoKHR& createInfo
) const {
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

void ExtFunctions::cmdTraceRaysKHR(
	vk::CommandBuffer commandBuffer,
	const vk::StridedDeviceAddressRegionKHR& raygenShaderBindingTable,
	const vk::StridedDeviceAddressRegionKHR& missShaderBindingTable,
	const vk::StridedDeviceAddressRegionKHR& hitShaderBindingTable,
	const vk::StridedDeviceAddressRegionKHR& callableShaderBindingTable,
	uint32_t width,
	uint32_t height,
	uint32_t depth
) const {
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