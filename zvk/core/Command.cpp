#include "Command.h"

NAMESPACE_BEGIN(zvk)

void CommandBuffer::submitAndWait() {
    cmd.end();
    auto submitInfo = vk::SubmitInfo().setCommandBuffers(cmd);
    mCtx->queues[mQueueIdx].queue.submit(submitInfo);
    mCtx->queues[mQueueIdx].queue.waitIdle();
}

std::vector<std::unique_ptr<CommandBuffer>> Command::createPrimary(const Context* ctx, QueueIdx queueIdx, uint32_t n) {
    auto allocInfo = vk::CommandBufferAllocateInfo()
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandPool(ctx->cmdPools[queueIdx])
        .setCommandBufferCount(n);

    auto cmds = ctx->device.allocateCommandBuffers(allocInfo);
    std::vector<std::unique_ptr<CommandBuffer>> ret(cmds.size());

    for (uint32_t i = 0; i < n; i++) {
        ret[i] = std::make_unique<CommandBuffer>(ctx, queueIdx);
        ret[i]->cmd = cmds[i];
        ret[i]->open = false;
    }
    return ret;
}

std::unique_ptr<CommandBuffer> Command::createOneTimeSubmit(const Context* ctx, QueueIdx queueIdx) {
    auto cmd = std::make_unique<CommandBuffer>(ctx, queueIdx);

    auto allocInfo = vk::CommandBufferAllocateInfo()
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandPool(ctx->cmdPools[queueIdx])
        .setCommandBufferCount(1);

    cmd->cmd = ctx->device.allocateCommandBuffers(allocInfo)[0];
    cmd->open = true;

    auto beginInfo = vk::CommandBufferBeginInfo()
        .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    cmd->cmd.begin(beginInfo);
    return cmd;
}

NAMESPACE_END(zvk)
