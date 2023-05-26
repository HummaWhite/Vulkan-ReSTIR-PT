#include "Command.h"

NAMESPACE_BEGIN(zvk)

void CommandBuffer::endAndSubmit(vk::Queue queue) {
    cmd.end();

    auto submitInfo = vk::SubmitInfo().setCommandBuffers(cmd);
    queue.submit(submitInfo);
    queue.waitIdle();

    mCtx->device.freeCommandBuffers(mPool, cmd);
}

CommandBuffer Command::beginOneTimeSubmit(const Context& ctx, vk::CommandPool pool) {
    CommandBuffer buffer(ctx, pool);

    auto allocInfo = vk::CommandBufferAllocateInfo()
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandPool(pool)
        .setCommandBufferCount(1);

    buffer.cmd = ctx.device.allocateCommandBuffers(allocInfo)[0];

    auto beginInfo = vk::CommandBufferBeginInfo()
        .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    buffer.cmd.begin(beginInfo);
    return buffer;
}

NAMESPACE_END(zvk)
