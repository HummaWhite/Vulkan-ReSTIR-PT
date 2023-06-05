#include "HostImage.h"
#include "util/Error.h"

NAMESPACE_BEGIN(zvk)

HostImage::~HostImage() {
	if (mData != nullptr) {
		delete[] mData;
	}
}

vk::Format HostImage::format() const {
	if (dataType == HostImageType::Int8) {
		switch (channels) {
		case 1:
			return vk::Format::eR8Srgb;
		case 2:
			return vk::Format::eR8G8Srgb;
		case 3:
			return vk::Format::eR8G8B8Srgb;
		case 4:
			return vk::Format::eR8G8B8A8Srgb;
		}
	}
	else {
		switch (channels) {
		case 1:
			return vk::Format::eR32Sfloat;
		case 2:
			return vk::Format::eR32G32Sfloat;
		case 3:
			return vk::Format::eR32G32B32Sfloat;
		case 4:
			return vk::Format::eR32G32B32A32Sfloat;
		}
	}
	return vk::Format::eUndefined;
}

uint8_t* addAlphaChannel(const uint8_t* data, int width, int height, HostImageType type) {
	size_t nPixels = size_t(width) * height;
	int dataWidth = (type == HostImageType::Int8) ? 1 : 4;

	auto data32 = reinterpret_cast<const float*>(data);
	auto newData = new uint8_t[nPixels * dataWidth * 4];
	auto newData32 = reinterpret_cast<float*>(newData);

	for (size_t i = 0; i < nPixels; i++) {
		if (type == HostImageType::Int8) {
			newData[i * 4 + 0] = data[i * 3 + 0];
			newData[i * 4 + 1] = data[i * 3 + 1];
			newData[i * 4 + 2] = data[i * 3 + 2];
			newData[i * 4 + 3] = 0xffu;
		}
		else {
			newData32[i * 4 + 0] = data32[i * 3 + 0];
			newData32[i * 4 + 1] = data32[i * 3 + 1];
			newData32[i * 4 + 2] = data32[i * 3 + 2];
			newData32[i * 4 + 3] = 1.f;
		}
	}
	return newData;
}

HostImage* HostImage::createFromFile(const File::path& path, HostImageType type, int channels) {
	Log::bracketLine<0>("Image " + path.generic_string());

	auto image = new HostImage();
	auto pathStr = path.generic_string();

	uint8_t* data = (type == HostImageType::Float32) ?
		reinterpret_cast<uint8_t*>(
			stbi_loadf(pathStr.c_str(), &image->width, &image->height, &image->channels, channels)
			) :
		stbi_load(pathStr.c_str(), &image->width, &image->height, &image->channels, channels);

	if (data == nullptr) {
		Log::bracketLine<1>("Failed to load");
		return nullptr;
	}
	image->dataType = type;

	if (image->channels == 3 && channels == 3) {
		Log::bracketLine<1>("No alpha channel, added");
		image->mData = addAlphaChannel(data, image->width, image->height, type);
		image->channels = 4;
	}
	else {
		image->channels = channels;

		int bytesPerChannel = (type == HostImageType::Float32) ? sizeof(float) : 1;
		size_t size = static_cast<size_t>(image->width) * image->height * channels * bytesPerChannel;
		image->mData = new uint8_t[size];
		memcpy(image->mData, data, size);
	}
	stbi_image_free(data);
	return image;
}

HostImage* HostImage::createEmpty(int width, int height, HostImageType type, int channels) {
	Log::check(channels <= 4 && channels >= 1, "Invalid image channel parameter");

	auto image = new HostImage();
	int bytesPerChannel = (type == HostImageType::Int8) ? 1 : sizeof(float);
	size_t size = static_cast<size_t>(width) * height * channels * bytesPerChannel;
	image->width = width;
	image->height = height;
	image->channels = channels;
	image->dataType = type;
	image->mData = new uint8_t[size];
	return image;
}

NAMESPACE_END(zvk)