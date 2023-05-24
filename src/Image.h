#pragma once

#include <stb_image.h>
#include <stb_image_write.h>

#include "Memory.h"
#include "util/File.h"

NAMESPACE_BEGIN(zvk)

enum class HostImageType {
	Int8, Float32
};

class HostImage {
public:
	HostImage() = default;
	~HostImage();

	void* data() { return mData; }

	template<typename T>
	T* data() { return reinterpret_cast<T*>(mData); }

	size_t byteSize() const {
		return size_t(width) * height * channels * (dataType == HostImageType::Int8 ? 1 : 4);
	}

	vk::Extent2D extent() const { return vk::Extent2D(width, height); }
	vk::Format format() const;

	static HostImage* createFromFile(const File::path& path, HostImageType type, int channels = 3);
	static HostImage* createEmpty(int width, int height, HostImageType type, int channels = 3);

public:
	int width, height;
	int channels;
	HostImageType dataType;

private:
	uint8_t* mData = nullptr;
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

public:
	vk::Image image;
	vk::ImageView imageView;
	vk::ImageType type;
	vk::Extent3D extent;
	vk::Sampler sampler;
	vk::DeviceMemory memory;

private:
	const Context* mCtx;
};

namespace Memory {
	vk::Image createImage2D(
		const Context& ctx, vk::Extent2D extent, vk::Format format,
		vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties,
		vk::DeviceMemory& memory);

	Image createImage2D(
		const Context& ctx, vk::Extent2D extent, vk::Format format,
		vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);
}

NAMESPACE_END(zvk)