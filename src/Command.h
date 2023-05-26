#pragma once

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <optional>
#include <vector>

#include "Context.h"

NAMESPACE_BEGIN(zvk)

class CommandBuffer {
public:
    CommandBuffer() = default;
    CommandBuffer(const Context& ctx, vk::CommandPool pool) : mCtx(&ctx), mPool(pool) {}
    void endAndSubmit(vk::Queue queue);

public:
    vk::CommandBuffer cmd;

private:
    const Context* mCtx;
    vk::CommandPool mPool;
};

namespace Command {
    CommandBuffer beginOneTimeSubmit(const Context& ctx, vk::CommandPool pool);
}

NAMESPACE_END(zvk)