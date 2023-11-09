#include "ShaderBindingTable.h"
#include "shader/HostDevice.h"
#include "core/ExtFunctions.h"
#include "core/DebugUtils.h"

NAMESPACE_BEGIN(zvk)

ShaderBindingTable::ShaderBindingTable(const Context* ctx, uint32_t numMissGroup, uint32_t numHitGroup, vk::Pipeline pipeline) :
	BaseVkObject(ctx)
{
	const auto& pipelineProps = ctx->instance()->rayTracingPipelineProperties;

	uint32_t numGroups = 1 + numMissGroup + numHitGroup;
	uint32_t handleSize = pipelineProps.shaderGroupHandleSize;

	uint32_t alignedHandleSize = zvk::align(pipelineProps.shaderGroupHandleSize, pipelineProps.shaderGroupHandleAlignment);
	uint32_t baseAlignment = pipelineProps.shaderGroupBaseAlignment;

	rayGenRegion.stride = zvk::align(alignedHandleSize, baseAlignment);
	rayGenRegion.size = rayGenRegion.stride;

	missRegion.stride = alignedHandleSize;
	missRegion.size = zvk::align(numMissGroup * alignedHandleSize, baseAlignment);

	hitRegion.stride = alignedHandleSize;
	hitRegion.size = zvk::align(numHitGroup * alignedHandleSize, baseAlignment);

	uint32_t dataSize = numGroups * handleSize;
	auto handles = zvk::ExtFunctions::getRayTracingShaderGroupHandlesKHR(mCtx->device, pipeline, 0, numGroups, dataSize);

	vk::DeviceSize SBTSize = rayGenRegion.size + missRegion.size + hitRegion.size + callableRegion.size;

	buffer = zvk::Memory::createBuffer(
		mCtx, SBTSize,
		vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress |
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		vk::MemoryAllocateFlagBits::eDeviceAddress
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, buffer->buffer, "shader binding table");

	buffer->mapMemory();

	vk::DeviceAddress SBTAddress = buffer->address();
	rayGenRegion.deviceAddress = SBTAddress;
	missRegion.deviceAddress = SBTAddress + rayGenRegion.size;
	hitRegion.deviceAddress = SBTAddress + rayGenRegion.size + missRegion.size;

	auto pDst = reinterpret_cast<uint8_t*>(buffer->data);
	auto pSrc = reinterpret_cast<uint8_t*>(handles.data());

	memcpy(pDst, pSrc, handleSize);

	pDst += rayGenRegion.size;
	pSrc += handleSize;

	for (uint32_t i = 0; i < numMissGroup; i++) {
		memcpy(pDst + i * alignedHandleSize, pSrc + i * handleSize, handleSize);
	}
	pDst += missRegion.size;
	pSrc += numMissGroup * handleSize;

	for (uint32_t i = 0; i < numHitGroup; i++) {
		memcpy(pDst + i * alignedHandleSize, pSrc + i * handleSize, handleSize);
	}
	buffer->unmapMemory();
}

NAMESPACE_END(zvk)
