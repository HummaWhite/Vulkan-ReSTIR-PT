#include "Image.h"
#include "util/Error.h"

Image::~Image() {
	if (mData != nullptr) {
		delete[] mData;
	}
}

Image* Image::createFromFile(const File::path& path, ImageDataType type, int channels) {
	auto image = new Image();
	auto pathStr = path.generic_string();

	uint8_t* data = (type == ImageDataType::Int8) ?
		stbi_load(pathStr.c_str(), &image->width, &image->height, &image->channels, channels) :
		reinterpret_cast<uint8_t*>(
			stbi_loadf(pathStr.c_str(), &image->width, &image->height, &image->channels, channels)
		);

	if (data == nullptr) {
		Log::bracketLine<0>("Image Failed to load from file: " + path.generic_string());
		return nullptr;
	}
	image->dataType = type;
	int bytesPerChannel = (type == ImageDataType::Int8) ? 1 : sizeof(float);
	size_t size = static_cast<size_t>(image->width) * image->height * channels * bytesPerChannel;

	image->mData = new uint8_t[size];
	memcpy(image->mData, data, size);
	stbi_image_free(data);
	return image;
}

Image* Image::createEmpty(int width, int height, ImageDataType type, int channels) {
	Log::check(channels <= 4 && channels >= 1, "Invalid image channel parameter");

	auto image = new Image();
	int bytesPerChannel = (type == ImageDataType::Int8) ? 1 : sizeof(float);
	size_t size = static_cast<size_t>(width) * height * channels * bytesPerChannel;
	image->width = width;
	image->height = height;
	image->channels = channels;
	image->dataType = type;
	image->mData = new uint8_t[size];
	return image;
}

NAMESPACE_BEGIN(zvk)


NAMESPACE_END(zvk)