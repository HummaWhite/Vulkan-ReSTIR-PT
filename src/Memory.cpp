#include "Memory.h"

NAMESPACE_BEGIN(zvk)

std::optional<uint32_t> findMemoryType(
	vk::PhysicalDeviceMemoryProperties physicalDeviceMemProps, 
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
	vk::Device device, vk::PhysicalDevice physicalDevice,
	vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::DeviceMemory& memory
) {
	auto bufferCreateInfo = vk::BufferCreateInfo()
		.setSize(size)
		.setUsage(usage)
		.setSharingMode(vk::SharingMode::eExclusive);

	auto buffer = device.createBuffer(bufferCreateInfo);
	auto requirements = device.getBufferMemoryRequirements(buffer);
	auto memTypeIndex = findMemoryType(physicalDevice.getMemoryProperties(), requirements, properties);

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
	vk::Device device, vk::PhysicalDevice physicalDevice,
	vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties
) {
	Buffer buffer;

	auto bufferCreateInfo = vk::BufferCreateInfo()
		.setSize(size)
		.setUsage(usage)
		.setSharingMode(vk::SharingMode::eExclusive);

	buffer.buffer = device.createBuffer(bufferCreateInfo);
	auto requirements = device.getBufferMemoryRequirements(buffer.buffer);
	auto memTypeIndex = findMemoryType(physicalDevice.getMemoryProperties(), requirements, properties);

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

Buffer Buffer::createTempTransfer(vk::Device device, vk::PhysicalDevice physicalDevice, vk::DeviceSize size) {
	return create(
		device, physicalDevice, size,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);
}

vk::Buffer Buffer::createDeviceLocal(
	vk::Device device, vk::PhysicalDevice physicalDevice, vk::CommandPool cmdPool, vk::Queue queue,
	const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::DeviceMemory& memory
) {
	auto transferBuf = createTempTransfer(device, physicalDevice, size);
	transferBuf.mapMemory(device);
	memcpy(transferBuf.mappedMemory, data, size);
	transferBuf.unmapMemory(device);

	auto localBuf = create(
		device, physicalDevice, size,
		vk::BufferUsageFlagBits::eTransferDst | usage,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		memory
	);

	copy(device, cmdPool, queue, localBuf, transferBuf.buffer, size);
	transferBuf.destroy(device);
	return localBuf;
}

Buffer Buffer::createDeviceLocal(
	vk::Device device, vk::PhysicalDevice physicalDevice, vk::CommandPool cmdPool, vk::Queue queue,
	const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage
) {
	auto transferBuf = createTempTransfer(device, physicalDevice, size);
	transferBuf.mapMemory(device);
	memcpy(transferBuf.mappedMemory, data, size);
	transferBuf.unmapMemory(device);

	auto localBuf = create(
		device, physicalDevice, size,
		vk::BufferUsageFlagBits::eTransferDst | usage,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	copy(device, cmdPool, queue, localBuf.buffer, transferBuf.buffer, size);
	transferBuf.destroy(device);
	return localBuf;
}

void Buffer::mapMemory(vk::Device device) {
	mappedMemory = device.mapMemory(memory, 0, size);
}

void Buffer::unmapMemory(vk::Device device) {
	device.unmapMemory(memory);
	mappedMemory = nullptr;
}

NAMESPACE_END(zvk)