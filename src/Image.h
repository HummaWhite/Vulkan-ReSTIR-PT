#pragma once

#include <stb_image.h>
#include <stb_image_write.h>

#include "Memory.h"
#include "util/File.h"

enum class ImageDataType {
	Int8, Float32
};

struct Image {
	Image() = default;
	~Image();

	void* data() { return mData; }

	template<typename T>
	T* data() { return reinterpret_cast<T*>(mData); }

	static Image* createFromFile(const File::path& path, ImageDataType type, int channels = 3);
	static Image* createEmpty(int width, int height, ImageDataType type, int channels = 3);

	int width, height;
	int channels;
	ImageDataType dataType;

private:
	uint8_t* mData = nullptr;
};

NAMESPACE_BEGIN(zvk)

struct Image {
	vk::Image image;
	vk::DeviceMemory memory;
};

NAMESPACE_END(zvk)