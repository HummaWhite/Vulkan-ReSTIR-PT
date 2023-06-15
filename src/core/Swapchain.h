#pragma once

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <optional>
#include <vector>

#include "Instance.h"
#include "Context.h"
#include "util/NamespaceDecl.h"

NAMESPACE_BEGIN(zvk)

class Swapchain : public BaseVkObject {
public:
	Swapchain(const Context* ctx, uint32_t width, uint32_t height, vk::Format format, bool computeTarget = false);
	~Swapchain() { destroy(); }
	void destroy();

	vk::SwapchainKHR swapchain() const { return mSwapchain; }
	vk::Format format() const { return mFormat; }
	vk::Extent2D extent() const { return mExtent; }
	uint32_t width() const { return mExtent.width; }
	uint32_t height() const { return mExtent.height; }
	const std::vector<vk::Image>& images() const { return mImages; }
	const std::vector<vk::ImageLayout>& imageLayouts() const { return mImageLayouts; }
	const std::vector<vk::ImageView>& imageViews() const { return mImageViews; }
	size_t numImages() const { return mImages.size(); }

	void changeImageLayout(
		uint32_t imageIdx, vk::ImageLayout newLayout,
		vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask,
		vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage);

private:
	void createSwapchain(const Instance* instance, uint32_t width, uint32_t height, bool computeTarget);
	std::tuple<vk::SurfaceFormatKHR, vk::PresentModeKHR> selectFormatAndMode(
		const std::vector<vk::SurfaceFormatKHR>& formats,
		const std::vector<vk::PresentModeKHR>& presentModes);

	void createImageViews();

private:
	vk::SwapchainKHR mSwapchain;
	vk::Format mFormat;
	vk::Extent2D mExtent;
	std::vector<vk::Image> mImages;
	std::vector<vk::ImageLayout> mImageLayouts;
	std::vector<vk::ImageView> mImageViews;
};

NAMESPACE_END(zvk)