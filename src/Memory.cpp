#include "Memory.h"

NAMESPACE_BEGIN(zvk)

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

vk::Buffer Buffer::create(
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

Buffer Buffer::create(
	vk::Device device, const vk::PhysicalDeviceMemoryProperties& memProps,
	vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties
) {
	Buffer buffer;

	auto bufferCreateInfo = vk::BufferCreateInfo()
		.setSize(size)
		.setUsage(usage)
		.setSharingMode(vk::SharingMode::eExclusive);

	buffer.buffer = device.createBuffer(bufferCreateInfo);
	auto requirements = device.getBufferMemoryRequirements(buffer.buffer);
	auto memTypeIndex = findMemoryType(memProps, requirements, properties);

	if (!memTypeIndex) {
		throw std::runtime_error("Required memory type not found");
	}

	auto allocInfo = vk::MemoryAllocateInfo()
		.setAllocationSize(requirements.size)
		.setMemoryTypeIndex(memTypeIndex.value());

	buffer.memory = device.allocateMemory(allocInfo);
	device.bindBufferMemory(buffer.buffer, buffer.memory, 0);

	buffer.size = size;
	buffer.realSize = requirements.size;
	buffer.properties = properties;
	buffer.usage = usage;

	return buffer;
}

Buffer Buffer::create(
	const Context& ctx, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties
) {
	return create(ctx.device, ctx.memProperties, size, usage, properties);
}

void Buffer::copy(
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

Buffer Buffer::createTempTransfer(
	vk::Device device, const vk::PhysicalDeviceMemoryProperties& memProps, vk::DeviceSize size
) {
	return create(
		device, memProps, size,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);
}

vk::Buffer Buffer::createDeviceLocal(
	vk::Device device, const vk::PhysicalDeviceMemoryProperties& memProps,
	vk::CommandPool cmdPool, vk::Queue queue,
	const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::DeviceMemory& memory
) {
	auto transferBuf = createTempTransfer(device, memProps, size);
	transferBuf.mapMemory(device);
	memcpy(transferBuf.mappedMem, data, size);
	transferBuf.unmapMemory(device);

	auto localBuf = create(
		device, memProps, size,
		vk::BufferUsageFlagBits::eTransferDst | usage,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		memory
	);

	copy(device, cmdPool, queue, localBuf, transferBuf.buffer, size);
	transferBuf.destroy(device);
	return localBuf;
}

Buffer Buffer::createDeviceLocal(
	vk::Device device, const vk::PhysicalDeviceMemoryProperties& memProps,
	vk::CommandPool cmdPool, vk::Queue queue,
	const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage
) {
	auto transferBuf = createTempTransfer(device, memProps, size);
	transferBuf.mapMemory(device);
	memcpy(transferBuf.mappedMem, data, size);
	transferBuf.unmapMemory(device);

	auto localBuf = create(
		device, memProps, size,
		vk::BufferUsageFlagBits::eTransferDst | usage,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	copy(device, cmdPool, queue, localBuf.buffer, transferBuf.buffer, size);
	transferBuf.destroy(device);
	return localBuf;
}

Buffer Buffer::createDeviceLocal(
	const Context& ctx, vk::CommandPool cmdPool,
	const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage
) {
	return createDeviceLocal(
		ctx.device, ctx.memProperties, cmdPool, ctx.graphicsQueue.queue,
		data, size, usage
	);
}

void Buffer::mapMemory(vk::Device device) {
	mappedMem = device.mapMemory(memory, 0, size);
}

void Buffer::unmapMemory(vk::Device device) {
	device.unmapMemory(memory);
	mappedMem = nullptr;
}

NAMESPACE_END(zvk)