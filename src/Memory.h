#pragma once

#include <vulkan/vulkan.hpp>

#include <iostream>
#include <optional>

#include "Context.h"

#include "util/NamespaceDecl.h"
#include "util/EnumBitField.h"

NAMESPACE_BEGIN(zvk)

class Buffer {
public:
	Buffer() : mCtx(nullptr) {}
	Buffer(const Context& ctx) : mCtx(&ctx) {}

	bool isHostVisible() const {
		return hasFlagBit(properties, vk::MemoryPropertyFlagBits::eHostVisible);
	}

	void destroy() {
		mCtx->device.destroy(buffer);
		mCtx->device.freeMemory(memory);
	}

	void mapMemory();
	void unmapMemory();

public:
	vk::Buffer buffer;
	vk::DeviceMemory memory;
	vk::DeviceSize size;
	vk::DeviceSize realSize;
	vk::MemoryPropertyFlags properties;
	vk::BufferUsageFlags usage;
	void* data = nullptr;

private:
	const Context* mCtx;
};

namespace Memory {
	std::optional<uint32_t> findMemoryType(
		const Context& context,
		vk::MemoryRequirements requirements,
		vk::MemoryPropertyFlags requestedProps);

	vk::Buffer createBuffer(
		vk::Device device, const vk::PhysicalDeviceMemoryProperties& memProps,
		vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::DeviceMemory& memory);

	Buffer createBuffer(
		const Context& ctx,
		vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);

	void copyBuffer(
		vk::Device device, vk::CommandPool cmdPool, vk::Queue queue,
		vk::Buffer dstBuffer, vk::Buffer srcBuffer, vk::DeviceSize size);

	vk::Buffer createTransferBuffer(
		vk::Device device, const vk::PhysicalDeviceMemoryProperties& memProps,
		vk::DeviceSize size, vk::DeviceMemory& memory);

	Buffer createTransferBuffer(const Context& ctx, vk::DeviceSize size);

	vk::Buffer createLocalBuffer(
		vk::Device device, const vk::PhysicalDeviceMemoryProperties& memProps,
		vk::CommandPool cmdPool, vk::Queue queue,
		const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::DeviceMemory& memory);

	Buffer createLocalBuffer(
		const Context& ctx, vk::CommandPool cmdPool,
		const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage);
}

NAMESPACE_END(zvk)