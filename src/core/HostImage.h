#pragma once

#include <vulkan/vulkan.hpp>

#include <stb_image.h>
#include <stb_image_write.h>

#include "util/File.h"
#include "util/NamespaceDecl.h"

NAMESPACE_BEGIN(zvk)

enum class HostImageType {
	Int8, Float32
};

class HostImage {
public:
	HostImage() = default;
	~HostImage();

	void* data() { return mData; }
	const void* data() const { return mData; }

	template<typename T>
	T* data() { return reinterpret_cast<T*>(mData); }

	template<typename T>
	const T* data() const { return reinterpret_cast<const T*>(mData); }

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

NAMESPACE_END(zvk)