#include "Memory.h"

std::optional<uint32_t> findMemoryType(
	vk::PhysicalDeviceMemoryProperties physicalDeviceMemProps, 
	vk::MemoryRequirements requirements, 
	vk::MemoryPropertyFlags requestedProps)
{
	for (uint32_t i = 0; i < physicalDeviceMemProps.memoryTypeCount; i++)
	{
		if ((requirements.memoryTypeBits & (1 << i) &&
			((requestedProps & physicalDeviceMemProps.memoryTypes[i].propertyFlags) == requestedProps)))
			return i;
	}
	return std::nullopt;
}

BufferMemory createBufferMemory(
	vk::Device device, vk::PhysicalDevice physicalDevice,
	vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
{
	auto bufferCreateInfo = vk::BufferCreateInfo()
		.setSize(size)
		.setUsage(usage)
		.setSharingMode(vk::SharingMode::eExclusive);

	auto buffer = device.createBuffer(bufferCreateInfo);
	auto requirements = device.getBufferMemoryRequirements(buffer);
	auto memTypeIndex = findMemoryType(physicalDevice.getMemoryProperties(), requirements, properties);

	if (!memTypeIndex)
		throw std::runtime_error("Required memory type not found");

	auto allocInfo = vk::MemoryAllocateInfo()
		.setAllocationSize(requirements.size)
		.setMemoryTypeIndex(memTypeIndex.value());

	auto memory = device.allocateMemory(allocInfo);
	device.bindBufferMemory(buffer, memory, 0);
	return { buffer, memory };
}

void copyBuffer(
	vk::Device device, vk::CommandPool cmdPool, vk::Queue queue,
	vk::Buffer dstBuffer, vk::Buffer srcBuffer, vk::DeviceSize size)
{
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

BufferMemory createDeviceLocalBufferMemory(
	vk::Device device, vk::PhysicalDevice physicalDevice, vk::CommandPool cmdPool, vk::Queue queue,
	const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage)
{
	auto [transferBuf, transferMem] = createBufferMemory(
		device, physicalDevice, size,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	auto transferMemHandle = device.mapMemory(transferMem, 0, size);
	memcpy(transferMemHandle, data, size);
	device.unmapMemory(transferMem);

	auto [localBuf, localMem] = createBufferMemory(
		device, physicalDevice, size,
		vk::BufferUsageFlagBits::eTransferDst | usage,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

	copyBuffer(device, cmdPool, queue, localBuf, transferBuf, size);
	device.destroyBuffer(transferBuf);
	device.freeMemory(transferMem);
	return { localBuf, localMem };
}
