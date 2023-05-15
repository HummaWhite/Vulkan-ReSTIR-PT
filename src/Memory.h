#pragma once

#include <vulkan/vulkan.hpp>

#include <iostream>
#include <optional>

struct BufferMemory
{
	vk::Buffer buffer;
	vk::DeviceMemory memory;
};

struct DeviceLocalBufferMemoryCreateInfo
{
	vk::Device device;
	vk::PhysicalDevice physicalDevice;
	vk::CommandPool cmdPool;
	vk::Queue queue;
	void* data;
	vk::DeviceSize size;
	vk::BufferUsageFlags usage;
};

std::optional<uint32_t> findMemoryType(
	vk::PhysicalDeviceMemoryProperties physicalDeviceMemProps,
	vk::MemoryRequirements requirements,
	vk::MemoryPropertyFlags requestedProps);

BufferMemory createBufferMemory(
	vk::Device device, vk::PhysicalDevice physicalDevice,
	vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);

void copyBuffer(
	vk::Device device, vk::CommandPool cmdPool, vk::Queue queue,
	vk::Buffer dstBuffer, vk::Buffer srcBuffer, vk::DeviceSize size);

BufferMemory createDeviceLocalBufferMemory(
	vk::Device device, vk::PhysicalDevice physicalDevice, vk::CommandPool cmdPool, vk::Queue queue,
	const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage);