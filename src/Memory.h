#pragma once

#include <vulkan/vulkan.hpp>

#include <iostream>
#include <optional>

#include "util/NamespaceDecl.h"
#include "util/EnumBitField.h"

NAMESPACE_BEGIN(zvk)

struct DeviceLocalBufferMemoryCreateInfo {
	vk::Device device;
	vk::PhysicalDevice physicalDevice;
	vk::CommandPool cmdPool;
	vk::Queue queue;
	void* data;
	vk::DeviceSize size;
	vk::BufferUsageFlags usage;
};

struct Buffer {
	bool isHostVisible() const {
		return hasFlagBit(properties, vk::MemoryPropertyFlagBits::eHostVisible);
	}

	void destroy(vk::Device device) {
		device.destroy(buffer);
		device.freeMemory(memory);
	}

	static vk::Buffer create(
		vk::Device device, vk::PhysicalDevice physicalDevice,
		vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::DeviceMemory& memory);

	static Buffer create(
		vk::Device device, vk::PhysicalDevice physicalDevice,
		vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);

	static void copy(
		vk::Device device, vk::CommandPool cmdPool, vk::Queue queue,
		vk::Buffer dstBuffer, vk::Buffer srcBuffer, vk::DeviceSize size);

	static Buffer createTempTransfer(vk::Device device, vk::PhysicalDevice physicalDevice, vk::DeviceSize size);

	static vk::Buffer createDeviceLocal(
		vk::Device device, vk::PhysicalDevice physicalDevice, vk::CommandPool cmdPool, vk::Queue queue,
		const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::DeviceMemory& memory);

	static Buffer createDeviceLocal(
		vk::Device device, vk::PhysicalDevice physicalDevice, vk::CommandPool cmdPool, vk::Queue queue,
		const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage);

	void writeBuffer(
		vk::Device device, vk::PhysicalDevice physicalDevice, vk::CommandPool cmdPool, vk::Queue queue,
		vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize size, const void* data);

	void mapMemory(vk::Device device);
	void unmapMemory(vk::Device device);

	vk::Buffer buffer;
	vk::DeviceMemory memory;
	vk::DeviceSize size;
	vk::DeviceSize realSize;
	vk::MemoryPropertyFlags properties;
	vk::BufferUsageFlags usage;
	void* mappedMemory = nullptr;
};

NAMESPACE_END(zvk)