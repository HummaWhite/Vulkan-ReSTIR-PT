#pragma once

#include <vulkan/vulkan.hpp>

#include <iostream>
#include <optional>

#include "Context.h"
#include "Command.h"
#include "HostImage.h"

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

class Image {
public:
	Image() : mCtx(nullptr) {}
	Image(const Context& ctx) : mCtx(&ctx) {}

	void destroy() {
		mCtx->device.destroyImageView(imageView);
		mCtx->device.freeMemory(memory);
		mCtx->device.destroyImage(image);
	}

	void transitLayout(vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

public:
	vk::Image image;
	vk::ImageView imageView;
	vk::ImageType type;
	vk::Format format;
	vk::Extent3D extent;
	vk::Sampler sampler;
	vk::DeviceMemory memory;

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
		const Context& ctx, QueueIdx queueIdx,
		const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage);

	vk::Image createImage2D(
		const Context& ctx, vk::Extent2D extent, vk::Format format,
		vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties,
		vk::DeviceMemory& memory);

	Image createImage2D(
		const Context& ctx, vk::Extent2D extent, vk::Format format,
		vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);

	Image createTexture2D(
		const Context& ctx, const HostImage* hostImg,
		vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);
}

NAMESPACE_END(zvk)