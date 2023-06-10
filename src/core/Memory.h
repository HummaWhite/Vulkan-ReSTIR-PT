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

class Buffer : public BaseVkObject {
public:
	Buffer() {}
	Buffer(const Context& ctx) : BaseVkObject(ctx) {}

	bool isHostVisible() const {
		return hasFlagBit(properties, vk::MemoryPropertyFlagBits::eHostVisible);
	}

	void destroy() {
		mCtx->device.destroy(buffer);
		mCtx->device.freeMemory(memory);
	}

	void mapMemory() {
		data = mCtx->device.mapMemory(memory, 0, size);
	}

	void unmapMemory() {
		mCtx->device.unmapMemory(memory);
		data = nullptr;
	}

public:
	vk::Buffer buffer;
	vk::DeviceMemory memory;
	vk::DeviceSize size;
	vk::DeviceSize realSize;
	vk::MemoryPropertyFlags properties;
	vk::BufferUsageFlags usage;
	void* data = nullptr;
};

class Image : public BaseVkObject {
public:
	Image() {}
	Image(const Context& ctx) : BaseVkObject(ctx) {}

	void destroy() {
		mCtx->device.destroySampler(sampler);
		mCtx->device.destroyImageView(imageView);
		mCtx->device.freeMemory(memory);
		mCtx->device.destroyImage(image);
	}

	void changeLayout(vk::ImageLayout newLayout);
	void createImageView(bool array = false);
	void createSampler(vk::Filter filter, bool anisotropyIfPossible = false);
	void createMipmap();

	static uint32_t mipLevels(vk::Extent2D extent) {
		return std::floor(std::log2(std::max(extent.width, extent.height))) + 1;
	}

	static uint32_t mipLevels(vk::Extent3D extent) {
		return std::floor(std::log2(std::max(std::max(extent.width, extent.height), extent.depth))) + 1;
	}

public:
	vk::Image image;
	vk::ImageType type;
	vk::ImageView imageView;
	vk::ImageViewType imageViewType;
	vk::ImageUsageFlags usage;
	vk::ImageLayout layout;
	vk::Format format;
	vk::Extent3D extent;
	vk::Sampler sampler;
	vk::DeviceMemory memory;
	uint32_t nMipLevels;
	uint32_t nArrayLayers;
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
		vk::DeviceMemory& memory, uint32_t nMipLevels = 1);

	Image createImage2D(
		const Context& ctx, vk::Extent2D extent, vk::Format format,
		vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties,
		uint32_t nMipLevels = 1);

	Image createTexture2D(
		const Context& ctx, const HostImage* hostImg,
		vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::ImageLayout layout, vk::MemoryPropertyFlags properties,
		uint32_t nMipLevels = 1);

	void copyBufferToImage(
		const Context& ctx, const Buffer& buffer, const Image& image);
}

NAMESPACE_END(zvk)