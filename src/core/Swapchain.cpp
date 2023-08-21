#include "Swapchain.h"
#include "Command.h"
#include "util/EnumBitField.h"
#include "shader/HostDevice.h"

#include <set>

NAMESPACE_BEGIN(zvk)

Swapchain::Swapchain(const Context* ctx, uint32_t width, uint32_t height, vk::Format format, bool computeTarget) :
	BaseVkObject(ctx), mFormat(format) {
	createSwapchain(ctx->instance(), width, height, computeTarget);
	createImageViews();
}

void Swapchain::destroy() {
	for (auto& imageView : mImageViews) {
		mCtx->device.destroyImageView(imageView);
	}
	mCtx->device.destroySwapchainKHR(mSwapchain);
}

void Swapchain::changeImageLayoutCmd(
	vk::CommandBuffer cmd, uint32_t imageIdx, vk::ImageLayout newLayout,
	vk::PipelineStageFlags srcStage, vk::AccessFlags srcAccessMask,
	vk::PipelineStageFlags dstStage, vk::AccessFlags dstAccessMask
) {
	auto barrier = vk::ImageMemoryBarrier()
		.setOldLayout(mImageLayouts[imageIdx])
		.setNewLayout(newLayout)
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setImage(mImages[imageIdx])
		.setSubresourceRange(vk::ImageSubresourceRange()
			.setAspectMask(vk::ImageAspectFlagBits::eColor)
			.setBaseMipLevel(0)
			.setLevelCount(1)
			.setBaseArrayLayer(0)
			.setLayerCount(1))
		.setSrcAccessMask(srcAccessMask)
		.setDstAccessMask(dstAccessMask);

	cmd.pipelineBarrier(srcStage, dstStage, vk::DependencyFlagBits{ 0 }, {}, {}, barrier);
	mImageLayouts[imageIdx] = newLayout;
}

void Swapchain::changeImageLayout(
	uint32_t imageIdx, vk::ImageLayout newLayout,
	vk::PipelineStageFlags srcStage, vk::AccessFlags srcAccessMask,
	vk::PipelineStageFlags dstStage, vk::AccessFlags dstAccessMask
) {
	auto cmd = Command::createOneTimeSubmit(mCtx, QueueIdx::GeneralUse);
	changeImageLayoutCmd(cmd->cmd, imageIdx, newLayout, srcStage, srcAccessMask, dstStage, dstAccessMask);
	cmd->submitAndWait();
}

void Swapchain::initImageLayoutAllCmd(
	vk::CommandBuffer cmd, vk::ImageLayout layout,
	vk::PipelineStageFlags srcStage, vk::AccessFlags srcAccessMask,
	vk::PipelineStageFlags dstStage, vk::AccessFlags dstAccessMask
) {
	std::vector<vk::ImageMemoryBarrier> barriers(
		mImages.size(),
		vk::ImageMemoryBarrier()
		.setOldLayout(vk::ImageLayout::eUndefined)
		.setNewLayout(layout)
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setSubresourceRange(
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1))
		.setSrcAccessMask(srcAccessMask)
		.setDstAccessMask(dstAccessMask)
	);

	for (uint32_t i = 0; i < barriers.size(); i++) {
		barriers[i].setImage(mImages[i]);
	}
	cmd.pipelineBarrier(srcStage, dstStage, vk::DependencyFlagBits{ 0 }, {}, {}, barriers);
	std::fill(mImageLayouts.begin(), mImageLayouts.end(), layout);
}

void Swapchain::initImageLayoutAll(
	vk::ImageLayout layout,
	vk::PipelineStageFlags srcStage, vk::AccessFlags srcAccessMask,
	vk::PipelineStageFlags dstStage, vk::AccessFlags dstAccessMask
) {
	auto cmd = Command::createOneTimeSubmit(mCtx, QueueIdx::GeneralUse);
	initImageLayoutAllCmd(cmd->cmd, layout, srcStage, srcAccessMask, dstStage, dstAccessMask);
	cmd->submitAndWait();
}

void Swapchain::createSwapchain(const Instance* instance, uint32_t width, uint32_t height, bool computeTarget) {
	auto physicalDevice = instance->physicalDevice();

	auto capabilities = physicalDevice.getSurfaceCapabilitiesKHR(instance->surface());
	auto formats = physicalDevice.getSurfaceFormatsKHR(instance->surface());
	auto presentModes = physicalDevice.getSurfacePresentModesKHR(instance->surface());

	auto [format, presentMode] = selectFormatAndMode(formats, presentModes);

	width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	mExtent = vk::Extent2D(width, height);

	if (capabilities.currentExtent.width != UINT32_MAX) {
		mExtent = capabilities.currentExtent;
	}

	uint32_t nImages = capabilities.minImageCount + 1;

	if (capabilities.maxImageCount) {
		nImages = std::min(nImages, capabilities.maxImageCount);
	}
	vk::ImageUsageFlags usage = computeTarget ? vk::ImageUsageFlagBits::eStorage : vk::ImageUsageFlagBits::eColorAttachment;

	if (!hasFlagBit(capabilities.supportedUsageFlags, usage)) {
		Log::exception("zvk::Swapchain image usage not supported");
	}

	uint32_t generalIdx = mCtx->queues[QueueIdx::GeneralUse].familyIdx;
	uint32_t presentIdx = mCtx->queues[QueueIdx::Present].familyIdx;

	bool sameQueueFamily = generalIdx == presentIdx;

	auto sharingMode = sameQueueFamily ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent;
	auto queueFamilyIndices = sameQueueFamily ? std::vector<uint32_t>() :
		std::vector<uint32_t>({ generalIdx, presentIdx });

	auto createInfo = vk::SwapchainCreateInfoKHR()
		.setSurface(instance->surface())
		.setMinImageCount(nImages)
		.setImageFormat(format.format)
		.setImageColorSpace(format.colorSpace)
		.setImageExtent(mExtent)
		.setImageArrayLayers(1)
		.setImageUsage(usage)
		.setImageSharingMode(sharingMode)
		.setQueueFamilyIndices(queueFamilyIndices)
		.setPreTransform(capabilities.currentTransform)
		.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
		.setPresentMode(presentMode)
		.setClipped(true);

	mSwapchain = mCtx->device.createSwapchainKHR(createInfo);
	mImages = mCtx->device.getSwapchainImagesKHR(mSwapchain);
	mImageLayouts.resize(mImages.size(), vk::ImageLayout::eUndefined);
}

std::tuple<vk::SurfaceFormatKHR, vk::PresentModeKHR> Swapchain::selectFormatAndMode(
	const std::vector<vk::SurfaceFormatKHR>& formats,
	const std::vector<vk::PresentModeKHR>& presentModes
) {
	vk::SurfaceFormatKHR format;
	vk::PresentModeKHR presentMode;

	for (const auto& i : formats) {
		if (i.format == mFormat && i.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
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
