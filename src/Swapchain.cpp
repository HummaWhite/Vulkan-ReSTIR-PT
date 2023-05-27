#include "Swapchain.h"

#include <set>

NAMESPACE_BEGIN(zvk)

Swapchain::Swapchain(const Context& ctx, uint32_t width, uint32_t height) :
	mCtx(&ctx) {
	createSwapchain(*ctx.instance(), width, height);
	createImageViews();
}

void Swapchain::destroy() {
	for (auto& imageView : mImageViews) {
		mCtx->device.destroyImageView(imageView);
	}
	mCtx->device.destroySwapchainKHR(mSwapchain);
}

void Swapchain::createSwapchain(const Instance& instance, uint32_t width, uint32_t height) {
	auto physicalDevice = instance.physicalDevice();

	auto capabilities = physicalDevice.getSurfaceCapabilitiesKHR(instance.surface());
	auto formats = physicalDevice.getSurfaceFormatsKHR(instance.surface());
	auto presentModes = physicalDevice.getSurfacePresentModesKHR(instance.surface());

	auto [format, presentMode] = selectFormatAndMode(formats, presentModes);

	width = std::clamp(width, capabilities.minImageExtent.width,
		capabilities.maxImageExtent.width);
	height = std::clamp(height, capabilities.minImageExtent.height,
		capabilities.maxImageExtent.height);
	mExtent = vk::Extent2D(width, height);

	if (capabilities.currentExtent.width != UINT32_MAX) {
		mExtent = capabilities.currentExtent;
	}

	uint32_t nImages = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount) {
		nImages = std::min(nImages, capabilities.maxImageCount);
	}

	uint32_t generalIdx = mCtx->queues[QueueIdx::GeneralUse].familyIdx;
	uint32_t presentIdx = mCtx->queues[QueueIdx::Present].familyIdx;

	bool sameQueueFamily = generalIdx == presentIdx;

	auto sharingMode = sameQueueFamily ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent;
	auto queueFamilyIndices = sameQueueFamily ? std::vector<uint32_t>() :
		std::vector<uint32_t>({ generalIdx, presentIdx });

	auto createInfo = vk::SwapchainCreateInfoKHR()
		.setSurface(instance.surface())
		.setMinImageCount(nImages)
		.setImageFormat(format.format)
		.setImageColorSpace(format.colorSpace)
		.setImageExtent(mExtent)
		.setImageArrayLayers(1)
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
		.setImageSharingMode(sharingMode)
		.setQueueFamilyIndices(queueFamilyIndices)
		.setPreTransform(capabilities.currentTransform)
		.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
		.setPresentMode(presentMode)
		.setClipped(true);

	mSwapchain = mCtx->device.createSwapchainKHR(createInfo);
	mImages = mCtx->device.getSwapchainImagesKHR(mSwapchain);
	mFormat = format.format;
}

std::tuple<vk::SurfaceFormatKHR, vk::PresentModeKHR> Swapchain::selectFormatAndMode(
	const std::vector<vk::SurfaceFormatKHR>& formats,
	const std::vector<vk::PresentModeKHR>& presentModes
) {
	vk::SurfaceFormatKHR format;
	vk::PresentModeKHR presentMode;

	for (const auto& i : formats) {
		if (i.format == vk::Format::eB8G8R8A8Srgb && i.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			format = i;
		}
	}
	format = formats[0];

	for (const auto& i : presentModes) {
		if (i == vk::PresentModeKHR::eMailbox) {
			presentMode = i;
		}
	}
	presentMode = vk::PresentModeKHR::eFifo;
	return { format, presentMode };
}

void Swapchain::createImageViews() {
	mImageViews.resize(mImages.size());

	for (size_t i = 0; i < mImages.size(); i++) {
		auto createInfo = vk::ImageViewCreateInfo()
			.setImage(mImages[i])
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(mFormat)
			.setComponents(vk::ComponentMapping())
			.setSubresourceRange(
				vk::ImageSubresourceRange()
				.setAspectMask(vk::ImageAspectFlagBits::eColor)
				.setBaseMipLevel(0)
				.setLevelCount(1)
				.setBaseArrayLayer(0)
				.setLayerCount(1)
			);
		mImageViews[i] = mCtx->device.createImageView(createInfo);
	}
}

NAMESPACE_END(zvk)
