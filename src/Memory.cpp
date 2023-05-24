#include "Memory.h"

NAMESPACE_BEGIN(zvk)

void Buffer::mapMemory() {
	data = mCtx->device.mapMemory(memory, 0, size);
}

void Buffer::unmapMemory() {
	mCtx->device.unmapMemory(memory);
	data = nullptr;
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
		const Context& context,
		vk::MemoryRequirements requirements,
		vk::MemoryPropertyFlags requestedProps
	) {
		return findMemoryType(context.memProperties, requirements, requestedProps);
	}

	vk::Buffer createBuffer(
		vk::Device device, const vk::PhysicalDeviceMemoryProperties& memProps,
		vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::DeviceMemory& memory
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

		auto allocInfo = vk::MemoryAllocateInfo()
			.setAllocationSize(requirements.size)
			.setMemoryTypeIndex(memTypeIndex.value());

		memory = device.allocateMemory(allocInfo);
		device.bindBufferMemory(buffer, memory, 0);

		return buffer;
	}

	Buffer createBuffer(
		const Context& ctx, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties
	) {
		Buffer buffer(ctx);

		auto bufferCreateInfo = vk::BufferCreateInfo()
			.setSize(size)
			.setUsage(usage)
			.setSharingMode(vk::SharingMode::eExclusive);

		buffer.buffer = ctx.device.createBuffer(bufferCreateInfo);
		auto requirements = ctx.device.getBufferMemoryRequirements(buffer.buffer);
		auto memTypeIndex = findMemoryType(ctx.memProperties, requirements, properties);

		if (!memTypeIndex) {
			throw std::runtime_error("Required memory type not found");
		}

		auto allocInfo = vk::MemoryAllocateInfo()
			.setAllocationSize(requirements.size)
			.setMemoryTypeIndex(memTypeIndex.value());

		buffer.memory = ctx.device.allocateMemory(allocInfo);
		ctx.device.bindBufferMemory(buffer.buffer, buffer.memory, 0);

		buffer.size = size;
		buffer.realSize = requirements.size;
		buffer.properties = properties;
		buffer.usage = usage;

		return buffer;
	}

	void copyBuffer(
		vk::Device device, vk::CommandPool cmdPool, vk::Queue queue,
		vk::Buffer dstBuffer, vk::Buffer srcBuffer, vk::DeviceSize size
	) {
		auto cmdBufferAllocInfo = vk::CommandBufferAllocateInfo()
			.setCommandPool(cmdPool)
			.setCommandBufferCount(1)
			.setLevel(vk::CommandBufferLevel::ePrimary);

		auto cmdBuffer = device.allocateCommandBuffers(cmdBufferAllocInfo)[0];
		auto cmdBeginInfo = vk::CommandBufferBeginInfo()
			.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);


		auto copyInfo = vk::BufferCopy().setSrcOffset(0).setDstOffset(0).setSize(size);

		cmdBuffer.begin(cmdBeginInfo);
		cmdBuffer.copyBuffer(srcBuffer, dstBuffer, copyInfo);
		cmdBuffer.end();

		auto submitInfo = vk::SubmitInfo().setCommandBuffers(cmdBuffer);

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

	Buffer createTransferBuffer(const Context& ctx, vk::DeviceSize size) {
		return createBuffer(
			ctx, size, vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
		);
	}

	vk::Buffer createLocalBuffer(
		vk::Device device, const vk::PhysicalDeviceMemoryProperties& memProps,
		vk::CommandPool cmdPool, vk::Queue queue,
		const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::DeviceMemory& memory
	) {
		vk::DeviceMemory transferMem;
		auto transferBuf = createTransferBuffer(device, memProps, size, transferMem);

		auto mem = device.mapMemory(transferMem, 0, size);
		memcpy(mem, data, size);
		device.unmapMemory(transferMem);

		auto localBuf = createBuffer(
			device, memProps, size,
			vk::BufferUsageFlagBits::eTransferDst | usage,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			memory
		);

		copyBuffer(device, cmdPool, queue, localBuf, transferBuf, size);
		device.destroyBuffer(transferBuf);
		return localBuf;
	}

	Buffer createLocalBuffer(
		const Context& ctx, vk::CommandPool cmdPool,
		const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage
	) {
		auto transferBuf = createTransferBuffer(ctx, size);
		transferBuf.mapMemory();
		memcpy(transferBuf.data, data, size);
		transferBuf.unmapMemory();

		auto localBuf = createBuffer(
			ctx, size,
			vk::BufferUsageFlagBits::eTransferDst | usage,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);

		copyBuffer(ctx.device, cmdPool, ctx.graphicsQueue.queue, localBuf.buffer, transferBuf.buffer, size);
		transferBuf.destroy();
		return localBuf;
	}
}

NAMESPACE_END(zvk)