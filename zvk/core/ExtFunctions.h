#pragma once

#include <vulkan/vulkan.hpp>
#include <util/NamespaceDecl.h>

NAMESPACE_BEGIN(zvk::ExtFunctions)

void load(vk::Instance instance);

vk::DebugUtilsMessengerEXT createDebugUtilsMessengerEXT(
	vk::Instance instance,
	const vk::DebugUtilsMessengerCreateInfoEXT& createInfo);

void destroyDebugUtilsMessenger(
	vk::Instance instance,
	vk::DebugUtilsMessengerEXT messenger);

vk::AccelerationStructureBuildSizesInfoKHR getAccelerationStructureBuildSizesKHR(
	vk::Device device,
	vk::AccelerationStructureBuildTypeKHR buildType,
	const vk::AccelerationStructureBuildGeometryInfoKHR& buildInfo,
	vk::ArrayProxy<const uint32_t> const& maxPrimitiveCounts);

vk::AccelerationStructureKHR createAccelerationStructureKHR(
	vk::Device device,
	const vk::AccelerationStructureCreateInfoKHR& createInfo);

void destroyAccelerationStructureKHR(
	vk::Device device,
	vk::AccelerationStructureKHR accelerationStructure);

vk::DeviceAddress getAccelerationStructureDeviceAddressKHR(
	vk::Device device,
	const vk::AccelerationStructureDeviceAddressInfoKHR& info);

void cmdBuildAccelerationStructuresKHR(
	vk::CommandBuffer commandBuffer,
	vk::ArrayProxy<const vk::AccelerationStructureBuildGeometryInfoKHR> const& infos,
	vk::ArrayProxy<const vk::AccelerationStructureBuildRangeInfoKHR* const> const& pBuildRangeInfos);

std::vector<uint8_t> getRayTracingShaderGroupHandlesKHR(
	vk::Device device,
	vk::Pipeline pipeline,
	uint32_t firstGroup,
	uint32_t groupCount,
	size_t dataSize);

vk::ResultValue<vk::Pipeline> createRayTracingPipelineKHR(
	vk::Device device,
	vk::DeferredOperationKHR deferredOperation,
	vk::PipelineCache pipelineCache,
	const vk::RayTracingPipelineCreateInfoKHR& createInfo);

void cmdTraceRaysKHR(
	vk::CommandBuffer commandBuffer,
	const vk::StridedDeviceAddressRegionKHR& raygenShaderBindingTable,
	const vk::StridedDeviceAddressRegionKHR& missShaderBindingTable,
	const vk::StridedDeviceAddressRegionKHR& hitShaderBindingTable,
	const vk::StridedDeviceAddressRegionKHR& callableShaderBindingTable,
	uint32_t width,
	uint32_t height,
	uint32_t depth);

void setDebugUtilsObjectTagEXT(
	vk::Device device,
	const vk::DebugUtilsObjectTagInfoEXT& tagInfo);

void setDebugUtilsObjectNameEXT(
	vk::Device device,
	const vk::DebugUtilsObjectNameInfoEXT& nameInfo);

void cmdBeginDebugUtilsLabelEXT(
	vk::CommandBuffer commandBuffer,
	const vk::DebugUtilsLabelEXT& label);

void cmdEndDebugUtilsLabelEXT(
	vk::CommandBuffer commandBuffer);

void cmdInsertDebugUtilsLabelEXT(
	vk::CommandBuffer commandBuffer,
	const vk::DebugUtilsLabelEXT& label);

NAMESPACE_END(zvk::ExtFunctions)