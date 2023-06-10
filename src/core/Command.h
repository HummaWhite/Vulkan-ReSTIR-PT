#pragma once

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <optional>
#include <vector>

#include "Context.h"

NAMESPACE_BEGIN(zvk)

class CommandBuffer : public BaseVkObject {
public:
    CommandBuffer(const Context* ctx, QueueIdx queueIdx) : BaseVkObject(ctx), mQueueIdx(queueIdx), open(false) {}
    ~CommandBuffer() { destroy(); }

    void destroy() {
        mCtx->device.freeCommandBuffers(mCtx->cmdPools[mQueueIdx], cmd);
    }

    void begin(const vk::CommandBufferBeginInfo& beginInfo) {
        cmd.begin(beginInfo);
    }

    void oneTimeSubmitAndDestroy();

public:
    vk::CommandBuffer cmd;
    bool open;

private:
    QueueIdx mQueueIdx;
};

namespace Command {
    std::vector<CommandBuffer*> createPrimary(const Context* ctx, QueueIdx queueIdx, size_t n);
    CommandBuffer* createOneTimeSubmit(const Context* ctx, QueueIdx queueIdx);
}

NAMESPACE_END(zvk)