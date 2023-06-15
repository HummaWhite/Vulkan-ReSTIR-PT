#include "Command.h"

NAMESPACE_BEGIN(zvk)

void CommandBuffer::oneTimeSubmit() {
    cmd.end();
    auto submitInfo = vk::SubmitInfo().setCommandBuffers(cmd);
    mCtx->queues[mQueueIdx].queue.submit(submitInfo);
    mCtx->queues[mQueueIdx].queue.waitIdle();
}

std::vector<CommandBuffer*> Command::createPrimary(const Context* ctx, QueueIdx queueIdx, size_t n) {
    auto allocInfo = vk::CommandBufferAllocateInfo()
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandPool(ctx->cmdPools[queueIdx])
        .setCommandBufferCount(n);

    auto cmds = ctx->device.allocateCommandBuffers(allocInfo);
    std::vector<CommandBuffer*> ret(cmds.size());

    for (size_t i = 0; i < n; i++) {
        ret[i] = new CommandBuffer(ctx, queueIdx);
        ret[i]->cmd = cmds[i];
        ret[i]->open = false;
    }
    return ret;
}

CommandBuffer* Command::createOneTimeSubmit(const Context* ctx, QueueIdx queueIdx) {
    auto cmd = new CommandBuffer(ctx, queueIdx);

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
