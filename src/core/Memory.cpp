#include "Memory.h"

NAMESPACE_BEGIN(zvk)

struct LayoutTransitFlags {
	vk::AccessFlags srcMask;
	vk::AccessFlags dstMask;
	vk::PipelineStageFlags srcStage;
	vk::PipelineStageFlags dstStage;
};

std::optional<LayoutTransitFlags> findLayoutTransitFlags(vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
	auto fromTo = [=](vk::ImageLayout old, vk::ImageLayout neo) {
		return oldLayout == old && newLayout == neo;
	};
	vk::AccessFlags dstMask;
	vk::PipelineStageFlags dstStage;

	// TODO: this may not work for all cases. Still have to manually set the states

	if (oldLayout == vk::ImageLayout::eUndefined) {
		if (newLayout == vk::ImageLayout::eTransferDstOptimal) {
			dstMask = vk::AccessFlagBits::eTransferWrite;
			dstStage = vk::PipelineStageFlagBits::eTransfer;
		}
		else if (newLayout == vk::ImageLayout::ePresentSrcKHR) {
			dstMask = vk::AccessFlagBits::eNone;
			dstStage = vk::PipelineStageFlagBits::eBottomOfPipe;
		}
		else if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
			dstMask = vk::AccessFlagBits::eDepthStencilAttachmentRead |
				vk::AccessFlagBits::eDepthStencilAttachmentWrite;
			dstStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
		}
		else if (newLayout == vk::ImageLayout::eColorAttachmentOptimal) {
			dstMask = vk::AccessFlagBits::eColorAttachmentWrite;
			dstStage = vk::PipelineStageFlagBits::eFragmentShader;
		}

		return LayoutTransitFlags{
			vk::AccessFlagBits::eNone, dstMask,
			vk::PipelineStageFlagBits::eTopOfPipe, dstStage
		};
	}
	else if (oldLayout == vk::ImageLayout::eTransferDstOptimal) {
		if (newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
			dstMask = vk::AccessFlagBits::eShaderRead;
			dstStage = vk::PipelineStageFlagBits::eFragmentShader |
				vk::PipelineStageFlagBits::eComputeShader |
				vk::PipelineStageFlagBits::eRayTracingShaderNV;
		}
		else {
			return std::nullopt;
		}

		return LayoutTransitFlags{
			vk::AccessFlagBits::eTransferWrite, dstMask,
			vk::PipelineStageFlagBits::eTransfer, dstStage
		};
	}
	return std::nullopt;
}

vk::ImageMemoryBarrier Image::getBarrier(
	vk::ImageLayout newLayout,
	vk::AccessFlags srcAccessMask, vk::AccessFlags dstAccessMask, QueueIdx srcQueueFamily, QueueIdx dstQueueFamily
) {
	uint32_t srcFamily = (srcQueueFamily == QueueIdx::Ignored) ?
		VK_QUEUE_FAMILY_IGNORED : mCtx->queues[srcQueueFamily].familyIdx;

	uint32_t dstFamily = (dstQueueFamily == QueueIdx::Ignored) ?
		VK_QUEUE_FAMILY_IGNORED : mCtx->queues[dstQueueFamily].familyIdx;

	auto barrier = vk::ImageMemoryBarrier()
		.setOldLayout(layout)
		.setNewLayout(newLayout)
		.setSrcQueueFamilyIndex(srcFamily)
		.setDstQueueFamilyIndex(dstFamily)
		.setImage(image)
		.setSubresourceRange(vk::ImageSubresourceRange()
			.setAspectMask(vk::ImageAspectFlagBits::eColor)
			.setBaseMipLevel(0)
			.setLevelCount(numMipLevels)
			.setBaseArrayLayer(0)
			.setLayerCount(numArrayLayers))
		.setSrcAccessMask(srcAccessMask)
		.setDstAccessMask(dstAccessMask);

	layout = newLayout;
	return barrier;
}

vk::ImageMemoryBarrier2 Image::getBarrier2(
	vk::ImageLayout newLayout,
	vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
	vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask,
	QueueIdx srcQueueFamily, QueueIdx dstQueueFamily
) {
	uint32_t srcFamily = (srcQueueFamily == QueueIdx::Ignored) ?
		VK_QUEUE_FAMILY_IGNORED : mCtx->queues[srcQueueFamily].familyIdx;

	uint32_t dstFamily = (dstQueueFamily == QueueIdx::Ignored) ?
		VK_QUEUE_FAMILY_IGNORED : mCtx->queues[dstQueueFamily].familyIdx;

	auto barrier = vk::ImageMemoryBarrier2()
		.setOldLayout(layout)
		.setNewLayout(newLayout)
		.setSrcQueueFamilyIndex(srcFamily)
		.setDstQueueFamilyIndex(dstFamily)
		.setImage(image)
		.setSubresourceRange(vk::ImageSubresourceRange()
			.setAspectMask(vk::ImageAspectFlagBits::eColor)
			.setBaseMipLevel(0)
			.setLevelCount(numMipLevels)
			.setBaseArrayLayer(0)
			.setLayerCount(numArrayLayers))
		.setSrcAccessMask(srcAccessMask)
		.setDstAccessMask(dstAccessMask)
		.setSrcStageMask(srcStageMask)
		.setDstStageMask(dstStageMask);

	layout = newLayout;
	return barrier;
}

void Image::changeLayoutCmd(
	vk::CommandBuffer cmd, vk::ImageLayout newLayout,
	vk::PipelineStageFlags srcStage, vk::AccessFlags srcAccessMask,
	vk::PipelineStageFlags dstStage, vk::AccessFlags dstAccessMask
) {
	// TODO: check for image arrays
	auto barrier = getBarrier(newLayout, srcAccessMask, dstAccessMask);
	cmd.pipelineBarrier(srcStage, dstStage, vk::DependencyFlagBits{ 0 }, {}, {}, barrier);
	layout = newLayout;
}

void Image::changeLayout(
	vk::ImageLayout newLayout,
	vk::PipelineStageFlags srcStage, vk::AccessFlags srcAccessMask,
	vk::PipelineStageFlags dstStage, vk::AccessFlags dstAccessMask
) {
	auto cmd = Command::createOneTimeSubmit(mCtx, QueueIdx::GeneralUse);
	changeLayoutCmd(cmd->cmd, newLayout, srcStage, srcAccessMask, dstStage, dstAccessMask);
	cmd->submitAndWait();
}

void Image::changeLayoutCmd(vk::CommandBuffer cmd, vk::ImageLayout newLayout) {
	auto transitFlags = findLayoutTransitFlags(layout, newLayout);

	if (!transitFlags) {
		throw std::runtime_error("image layout transition not supported");
	}
	changeLayoutCmd(
		cmd, newLayout, transitFlags->srcStage, transitFlags->srcMask, transitFlags->dstStage, transitFlags->dstMask
	);
}

void Image::changeLayout(vk::ImageLayout newLayout) {
	auto transitFlags = findLayoutTransitFlags(layout, newLayout);

	if (!transitFlags) {
		throw std::runtime_error("image layout transition not supported");
	}
	changeLayout(
		newLayout, transitFlags->srcStage, transitFlags->srcMask, transitFlags->dstStage, transitFlags->dstMask
	);
}

vk::SamplerCreateInfo Image::samplerCreateInfo(vk::Filter filter, bool anisotropyIfPossible) {
	// TODO: check for image arrays

	return vk::SamplerCreateInfo()
		.setMinFilter(filter)
		.setMagFilter(filter)
		.setAddressModeU(vk::SamplerAddressMode::eRepeat)
		.setAddressModeV(vk::SamplerAddressMode::eRepeat)
		.setAddressModeW(vk::SamplerAddressMode::eRepeat)
		.setUnnormalizedCoordinates(false)
		.setAnisotropyEnable(mCtx->instance()->deviceFeatures.samplerAnisotropy & anisotropyIfPossible)
		.setMaxAnisotropy(mCtx->instance()->deviceProperties.limits.maxSamplerAnisotropy)
		.setCompareEnable(false)
		.setMipmapMode((filter == vk::Filter::eLinear) ? vk::SamplerMipmapMode::eLinear : vk::SamplerMipmapMode::eNearest)
		.setMipLodBias(0)
		.setMinLod(0)
		.setMaxLod(numMipLevels);
}

void Image::createImageView(bool array) {
	if (type == vk::ImageType::e1D) {
		viewType = vk::ImageViewType::e1D;
		numArrayLayers = 1;
	}
	else if (type == vk::ImageType::e2D) {
		viewType = array ? vk::ImageViewType::e1DArray : vk::ImageViewType::e2D;
		numArrayLayers = array ? extent.height : 1;
	}
	else {
		viewType = array ? vk::ImageViewType::e2DArray : vk::ImageViewType::e3D;
		numArrayLayers = array ? extent.depth : 1;
	}

	vk::ImageAspectFlags aspect = (usage == vk::ImageUsageFlagBits::eDepthStencilAttachment) ?
		aspect = vk::ImageAspectFlagBits::eDepth : aspect = vk::ImageAspectFlagBits::eColor;

	// TODO: check for image arrays

	auto createInfo = vk::ImageViewCreateInfo()
		.setImage(image)
		.setViewType(viewType)
		.setFormat(format)
		.setSubresourceRange(vk::ImageSubresourceRange()
			.setAspectMask(aspect)
			.setBaseMipLevel(0)
			.setLevelCount(numMipLevels)
			.setBaseArrayLayer(0)
			.setLayerCount(numArrayLayers));

	view = mCtx->device.createImageView(createInfo);
}

void Image::createSampler(vk::Filter filter, bool anisotropyIfPossible) {
	sampler = mCtx->device.createSampler(samplerCreateInfo(filter, anisotropyIfPossible));
}

void Image::createMipmap() {
	if (type != vk::ImageType::e2D) {
		throw std::runtime_error("mipmap not implemented for 1D and 3D images");
	}

	if (numMipLevels < 2) {
		return;
	}
	auto cmd = Command::createOneTimeSubmit(mCtx, QueueIdx::GeneralUse);

	auto barrier = vk::ImageMemoryBarrier()
		.setImage(image)
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setSubresourceRange(vk::ImageSubresourceRange()
			.setAspectMask(vk::ImageAspectFlagBits::eColor)
			.setBaseArrayLayer(0)
			.setLayerCount(1)
			.setLevelCount(1));

	int width = extent.width;
	int height = extent.height;

	// TODO: generate mipmap level data

	cmd->submitAndWait();
}

namespace Memory {
	std::optional<uint32_t> findMemoryType(
		const vk::PhysicalDeviceMemoryProperties& physicalDeviceMemProps,
		vk::MemoryRequirements requirements,
		vk::MemoryPropertyFlags requestedProps
	) {
		for (uint32_t i = 0; i < physicalDeviceMemProps.memoryTypeCount; i++) {
			if ((requirements.memoryTypeBits & (1 << i) &&
				((requestedProps & physicalDeviceMemProps.memoryTypes[i].propertyFlags) == requestedProps))) {
				return i;
			}
		}
		return std::nullopt;
	}

	std::optional<uint32_t> findMemoryType(
		const Context* ctx,
		vk::MemoryRequirements requirements,
		vk::MemoryPropertyFlags requestedProps
	) {
		return findMemoryType(ctx->instance()->memProperties, requirements, requestedProps);
	}

	vk::Buffer createBuffer(
		vk::Device device, const vk::PhysicalDeviceMemoryProperties& memProps,
		vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::DeviceMemory& memory,
		vk::MemoryAllocateFlags allocFlags
	) {
		auto bufferCreateInfo = vk::BufferCreateInfo()
			.setSize(size)
			.setUsage(usage)
			.setSharingMode(vk::SharingMode::eExclusive);

		auto buffer = device.createBuffer(bufferCreateInfo);
		auto requirements = device.getBufferMemoryRequirements(buffer);
		auto memTypeIndex = findMemoryType(memProps, requirements, properties);

		if (!memTypeIndex) {
			throw std::runtime_error("Required memory type not found");
		}

		auto allocFlagInfo = vk::MemoryAllocateFlagsInfo()
			.setFlags(allocFlags);

		auto allocInfo = vk::MemoryAllocateInfo()
			.setAllocationSize(requirements.size)
			.setMemoryTypeIndex(memTypeIndex.value())
			.setPNext((allocFlags == vk::MemoryAllocateFlags{ 0 }) ? nullptr : &allocFlagInfo);

		memory = device.allocateMemory(allocInfo);
		device.bindBufferMemory(buffer, memory, 0);

		return buffer;
	}

	std::unique_ptr<Buffer> createBuffer(
		const Context* ctx, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties,
		vk::MemoryAllocateFlags allocFlags
	) {
		auto buffer = std::make_unique<Buffer>(ctx);

		auto bufferCreateInfo = vk::BufferCreateInfo()
			.setSize(size)
			.setUsage(usage)
			.setSharingMode(vk::SharingMode::eExclusive);

		buffer->buffer = ctx->device.createBuffer(bufferCreateInfo);
		auto requirements = ctx->device.getBufferMemoryRequirements(buffer->buffer);
		auto memTypeIndex = findMemoryType(ctx->instance()->memProperties, requirements, properties);

		if (!memTypeIndex) {
			throw std::runtime_error("Required memory type not found");
		}

		auto allocFlagInfo = vk::MemoryAllocateFlagsInfo()
			.setFlags(allocFlags);

		auto allocInfo = vk::MemoryAllocateInfo()
			.setAllocationSize(requirements.size)
			.setMemoryTypeIndex(memTypeIndex.value())
			.setPNext((allocFlags == vk::MemoryAllocateFlags{ 0 }) ? nullptr : &allocFlagInfo);

		buffer->memory = ctx->device.allocateMemory(allocInfo);
		ctx->device.bindBufferMemory(buffer->buffer, buffer->memory, 0);

		buffer->size = size;
		buffer->realSize = requirements.size;
		buffer->properties = properties;
		buffer->usage = usage;

		return buffer;
	}

	void copyBufferCmd(vk::CommandBuffer cmd, vk::Buffer dstBuffer, vk::Buffer srcBuffer, vk::DeviceSize size) {
		cmd.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));
	}

	void copyBuffer(
		vk::Device device, vk::CommandPool cmdPool, vk::Queue queue,
		vk::Buffer dstBuffer, vk::Buffer srcBuffer, vk::DeviceSize size
	) {
		auto cmdBufferAllocInfo = vk::CommandBufferAllocateInfo()
			.setCommandPool(cmdPool)
			.setCommandBufferCount(1)
			.setLevel(vk::CommandBufferLevel::ePrimary);

		auto cmd = device.allocateCommandBuffers(cmdBufferAllocInfo)[0];
		auto cmdBeginInfo = vk::CommandBufferBeginInfo()
			.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

		auto copyInfo = vk::BufferCopy().setSrcOffset(0).setDstOffset(0).setSize(size);

		cmd.begin(cmdBeginInfo);
		cmd.copyBuffer(srcBuffer, dstBuffer, copyInfo);
		cmd.end();

		auto submitInfo = vk::SubmitInfo().setCommandBuffers(cmd);

		queue.submit(submitInfo);
		queue.waitIdle();
	}

	vk::Buffer createTransferBuffer(
		vk::Device device, const vk::PhysicalDeviceMemoryProperties& memProps,
		vk::DeviceSize size, vk::DeviceMemory& memory
	) {
		return createBuffer(
			device, memProps, size,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, memory
		);
	}

	std::unique_ptr<Buffer> createTransferBuffer(const Context* ctx, vk::DeviceSize size) {
		return createBuffer(
			ctx, size, vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
	}

	vk::Buffer createBufferFromHost(
		vk::Device device, const vk::PhysicalDeviceMemoryProperties& memProps,
		vk::CommandPool cmdPool, vk::Queue queue,
		const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::DeviceMemory& memory,
		vk::MemoryAllocateFlags allocFlags
	) {
		vk::DeviceMemory transferMem;
		auto transferBuf = createTransferBuffer(device, memProps, size, transferMem);

		auto mem = device.mapMemory(transferMem, 0, size);
		memcpy(mem, data, size);
		device.unmapMemory(transferMem);

		auto localBuf = createBuffer(
			device, memProps, size,
			vk::BufferUsageFlagBits::eTransferDst | usage, vk::MemoryPropertyFlagBits::eDeviceLocal,
			memory, allocFlags
		);

		copyBuffer(device, cmdPool, queue, localBuf, transferBuf, size);
		device.destroyBuffer(transferBuf);
		return localBuf;
	}

	std::unique_ptr<Buffer> createBufferFromHostCmd(
		vk::CommandBuffer cmd, const Context* ctx,
		const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage,
		vk::MemoryAllocateFlags allocFlags
	) {
		auto transferBuf = createTransferBuffer(ctx, size);
		transferBuf->mapMemory();
		memcpy(transferBuf->data, data, size);
		transferBuf->unmapMemory();

		auto localBuf = createBuffer(
			ctx, size,
			vk::BufferUsageFlagBits::eTransferDst | usage, vk::MemoryPropertyFlagBits::eDeviceLocal, allocFlags
		);
		copyBufferCmd(cmd, localBuf->buffer, transferBuf->buffer, size);

		auto barrier = vk::MemoryBarrier(vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eMemoryRead);
		
		cmd.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags{ 0 },
			barrier, {}, {}
		);
		return localBuf;
	}

	std::unique_ptr<Buffer> createBufferFromHost(
		const Context* ctx, QueueIdx queueIdx,
		const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryAllocateFlags allocFlags
	) {
		auto transferBuf = createTransferBuffer(ctx, size);
		transferBuf->mapMemory();
		memcpy(transferBuf->data, data, size);
		transferBuf->unmapMemory();

		auto localBuf = createBuffer(
			ctx, size,
			vk::BufferUsageFlagBits::eTransferDst | usage, vk::MemoryPropertyFlagBits::eDeviceLocal, allocFlags
		);

		copyBuffer(
			ctx->device, ctx->cmdPools[queueIdx], ctx->queues[queueIdx].queue,
			localBuf->buffer, transferBuf->buffer, size
		);
		return localBuf;
	}

	vk::Image createImage2D(
		const Context* ctx, vk::Extent2D extent, vk::Format format,
		vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties,
		vk::DeviceMemory& memory, uint32_t nMipLevels
	) {
		nMipLevels = std::min(nMipLevels, Image::mipLevels(extent));

		if (nMipLevels > 1) {
			usage |= vk::ImageUsageFlagBits::eTransferSrc;
		}
		auto createInfo = vk::ImageCreateInfo()
			.setImageType(vk::ImageType::e2D)
			.setExtent({ extent.width, extent.height, 1 })
			.setMipLevels(nMipLevels)
			.setArrayLayers(1)
			.setFormat(format)
			.setTiling(vk::ImageTiling::eOptimal)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setUsage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
			.setSharingMode(vk::SharingMode::eExclusive)
			.setSamples(vk::SampleCountFlagBits::e1);

		auto image = ctx->device.createImage(createInfo);
		auto memReq = ctx->device.getImageMemoryRequirements(image);

		auto memoryTypeIdx = findMemoryType(
			ctx, memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		auto allocInfo = vk::MemoryAllocateInfo()
			.setAllocationSize(memReq.size)
			.setMemoryTypeIndex(*memoryTypeIdx);

		memory = ctx->device.allocateMemory(allocInfo);
		ctx->device.bindImageMemory(image, memory, 0);

		return image;
	}

	std::unique_ptr<Image> createImage2D(
		const Context* ctx, vk::Extent2D extent, vk::Format format,
		vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, uint32_t nMipLevels
	) {
		auto image = std::make_unique<Image>(ctx);
		nMipLevels = std::min(nMipLevels, Image::mipLevels(extent));

		if (nMipLevels > 1) {
			usage |= vk::ImageUsageFlagBits::eTransferSrc;
		}
		auto createInfo = vk::ImageCreateInfo()
			.setImageType(vk::ImageType::e2D)
			.setExtent({ extent.width, extent.height, 1 })
			.setMipLevels(nMipLevels)
			.setArrayLayers(1)
			.setFormat(format)
			.setTiling(tiling)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setUsage(usage)
			.setSharingMode(vk::SharingMode::eExclusive)
			.setSamples(vk::SampleCountFlagBits::e1);

		image->image = ctx->device.createImage(createInfo);
		auto memReq = ctx->device.getImageMemoryRequirements(image->image);
		auto memoryTypeIdx = findMemoryType(ctx, memReq, properties);

		if (!memoryTypeIdx) {
			throw std::runtime_error("image memory type not found");
		}

		auto allocInfo = vk::MemoryAllocateInfo()
			.setAllocationSize(memReq.size)
			.setMemoryTypeIndex(*memoryTypeIdx);

		image->memory = ctx->device.allocateMemory(allocInfo);
		ctx->device.bindImageMemory(image->image, image->memory, 0);

		image->extent = vk::Extent3D(extent.width, extent.height, 1);
		image->type = vk::ImageType::e2D;
		image->usage = usage;
		image->layout = vk::ImageLayout::eUndefined;
		image->format = format;
		image->numMipLevels = nMipLevels;
		image->numArrayLayers = 1;
		return image;
	}

	std::unique_ptr<Image> createTexture2DCmd(
		vk::CommandBuffer cmd, const Context* ctx, const HostImage* hostImg,
		vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::ImageLayout layout, vk::MemoryPropertyFlags properties,
		uint32_t nMipLevels
	) {
		auto image = createImage2D(ctx, hostImg->extent(), hostImg->format(), tiling, usage, properties, nMipLevels);

		auto transferBuf = createBuffer(ctx, hostImg->byteSize(),
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);

		transferBuf->mapMemory();
		memcpy(transferBuf->data, hostImg->data(), hostImg->byteSize());
		transferBuf->unmapMemory();

		image->changeLayoutCmd(cmd, vk::ImageLayout::eTransferDstOptimal);
		copyBufferToImageCmd(cmd, transferBuf.get(), image.get());

		image->changeLayoutCmd(cmd, layout);
		image->createMipmap();
		image->createImageView();
		return image;
	}

	std::unique_ptr<Image> createTexture2D(
		const Context* ctx, QueueIdx queueIdx, const HostImage* hostImg,
		vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::ImageLayout layout, vk::MemoryPropertyFlags properties,
		uint32_t nMipLevels
	) {
		auto image = createImage2D(ctx, hostImg->extent(), hostImg->format(), tiling, usage, properties, nMipLevels);

		auto transferBuf = createBuffer(ctx, hostImg->byteSize(),
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);

		transferBuf->mapMemory();
		memcpy(transferBuf->data, hostImg->data(), hostImg->byteSize());
		transferBuf->unmapMemory();

		image->changeLayout(vk::ImageLayout::eTransferDstOptimal);
		copyBufferToImage(ctx, queueIdx, transferBuf.get(), image.get());

		image->changeLayout(layout);
		image->createMipmap();
		image->createImageView();
		return image;
	}

	void copyBufferToImageCmd(vk::CommandBuffer cmd, const Buffer* buffer, Image* image) {
		auto copy = vk::BufferImageCopy()
			.setBufferOffset(0)
			.setBufferRowLength(0)
			.setBufferImageHeight(0)
			.setImageSubresource(vk::ImageSubresourceLayers()
				.setAspectMask(vk::ImageAspectFlagBits::eColor)
				.setMipLevel(0)
				.setBaseArrayLayer(0)
				.setLayerCount(1))
			.setImageOffset({ 0, 0, 0 })
			.setImageExtent(image->extent);

		cmd.copyBufferToImage(buffer->buffer, image->image, image->layout, copy);
	}

	void copyBufferToImage(const Context* ctx, QueueIdx queueIdx, const Buffer* buffer, Image* image) {
		auto cmd = Command::createOneTimeSubmit(ctx, queueIdx);
		copyBufferToImageCmd(cmd->cmd, buffer, image);
		cmd->submitAndWait();
	}
}

NAMESPACE_END(zvk)
