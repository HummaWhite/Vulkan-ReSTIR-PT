#pragma once

#include <vulkan/vulkan.hpp>

#include <iostream>
#include <optional>
#include <array>

#include <util/EnumBitField.h>

#include "Context.h"
#include "Command.h"
#include "HostImage.h"

NAMESPACE_BEGIN(zvk)

template<typename T>
size_t sizeOf(const std::vector<T>& array) {
	return array.size() * sizeof(T);
}

template<typename T, size_t N>
size_t sizeOf(const std::array<T, N>& array) {
	return array.size() * sizeof(T);
}

template<typename T>
size_t sizeOf(const vk::ArrayProxy<const T>& array) {
	return array.size() * sizeof(T);
}

template<typename T>
size_t sizeOf(const vk::ArrayProxyNoTemporaries<const T>& array) {
	return array.size() * sizeof(T);
}

class Buffer : public BaseVkObject {
public:
	Buffer(const Context* ctx) : BaseVkObject(ctx) {}
	~Buffer() { destroy(); }

	void destroy() {
		mCtx->device.destroy(buffer);
		mCtx->device.freeMemory(memory);
	}

	vk::BufferMemoryBarrier getBufferBarrier(
		vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask,
		QueueIdx srcQueueFamily = QueueIdx::Ignored, QueueIdx dstQueueFamily = QueueIdx::Ignored);

	bool isHostVisible() const {
		return hasFlagBit(properties, vk::MemoryPropertyFlagBits::eHostVisible);
	}

	vk::DeviceAddress address() const {
		return mCtx->device.getBufferAddress({ buffer });
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
	vk::DeviceSize size = 0;
	vk::DeviceSize realSize = 0;
	vk::MemoryPropertyFlags properties;
	vk::BufferUsageFlags usage;
	void* data = nullptr;
};

class Image : public BaseVkObject {
public:
	Image(const Context* ctx) : BaseVkObject(ctx) {}
	~Image() { destroy(); }

	void destroy() {
		mCtx->device.destroySampler(sampler);
		mCtx->device.destroyImageView(view);
		mCtx->device.freeMemory(memory);
		mCtx->device.destroyImage(image);
	}

	vk::ImageMemoryBarrier getImageBarrierAndSetLayout(
		vk::ImageLayout newLayout,
		vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask,
		QueueIdx srcQueueFamily = QueueIdx::Ignored, QueueIdx dstQueueFamily = QueueIdx::Ignored);

	void changeLayoutCmd(
		vk::CommandBuffer cmd, vk::ImageLayout newLayout,
		vk::PipelineStageFlags srcStage, vk::AccessFlags srcAccessMask,
		vk::PipelineStageFlags dstStage, vk::AccessFlags dstAccessMask);

	void changeLayout(
		vk::ImageLayout newLayout,
		vk::PipelineStageFlags srcStage, vk::AccessFlags srcAccessMask,
		vk::PipelineStageFlags dstStage, vk::AccessFlags dstAccessMask);

	vk::SamplerCreateInfo samplerCreateInfo(vk::Filter filter, bool anisotropyIfPossible = false);

	void createImageView(bool array = false);
	void createSampler(vk::Filter filter, bool anisotropyIfPossible = false);
	void createMipmap();

	static uint32_t mipLevels(vk::Extent2D extent) {
		return static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height))) + 1);
	}

	static uint32_t mipLevels(vk::Extent3D extent) {
		return static_cast<uint32_t>(std::floor(std::log2(std::max(std::max(extent.width, extent.height), extent.depth))) + 1);
	}

public:
	vk::Image image;
	vk::ImageType type;
	vk::ImageView view;
	vk::ImageViewType viewType;
	vk::ImageUsageFlags usage;
	vk::ImageLayout layout;
	vk::Format format;
	vk::Extent3D extent;
	vk::Sampler sampler;
	vk::DeviceMemory memory;
	uint32_t numMipLevels;
	uint32_t numArrayLayers;
};

namespace Memory {
	std::unique_ptr<Buffer> createBuffer(
		const Context* ctx, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties,
		vk::MemoryAllocateFlags allocFlags = vk::MemoryAllocateFlags{ 0 });

	void copyBufferCmd(vk::CommandBuffer cmd, vk::Buffer dstBuffer, vk::Buffer srcBuffer, vk::DeviceSize size);

	std::unique_ptr<Buffer> createTransferBuffer(const Context* ctx, vk::DeviceSize size);

	std::unique_ptr<Buffer> createBufferFromHost(
		const Context* ctx, QueueIdx queueIdx,
		const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage,
		vk::MemoryAllocateFlags allocFlags = vk::MemoryAllocateFlags{ 0 });

	template<typename T>
	std::unique_ptr<Buffer> createBufferFromHost(
		const Context* ctx, QueueIdx queueIdx,
		const vk::ArrayProxy<const T>& data, vk::BufferUsageFlags usage,
		vk::MemoryAllocateFlags allocFlags = vk::MemoryAllocateFlags{ 0 }
	) {
		return createBufferFromHost(ctx, queueIdx, data.data(), sizeof(T) * data.size(), usage, allocFlags);
	}

	std::unique_ptr<Image> createImage2D(
		const Context* ctx, vk::Extent2D extent,
		vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
		vk::MemoryPropertyFlags properties,
		uint32_t nMipLevels = 1);

	std::unique_ptr<Image> createImage2DAndInitLayoutCmd(
		const Context* ctx, vk::CommandBuffer cmd, vk::Extent2D extent,
		vk::Format format, vk::ImageTiling tiling, vk::ImageLayout layout, vk::ImageUsageFlags usage,
		vk::MemoryPropertyFlags properties,
		uint32_t nMipLevels = 1);

	std::unique_ptr<Image> createImage2DAndInitLayout(
		const Context* ctx, QueueIdx queueIdx, vk::Extent2D extent,
		vk::Format format, vk::ImageTiling tiling, vk::ImageLayout layout, vk::ImageUsageFlags usage,
		vk::MemoryPropertyFlags properties,
		uint32_t nMipLevels = 1);

	std::unique_ptr<Image> createTexture2D(
		const Context* ctx, QueueIdx queueIdx,
		const HostImage* hostImg,
		vk::ImageTiling tiling, vk::ImageLayout layout, vk::ImageUsageFlags usage,
		vk::MemoryPropertyFlags properties,
		uint32_t nMipLevels = 1);

	void copyBufferToImageCmd(vk::CommandBuffer cmd, const Buffer* buffer, Image* image);

	void copyBufferToImage(const Context* ctx, QueueIdx queueIdx, const Buffer* buffer, Image* image);
}

NAMESPACE_END(zvk)