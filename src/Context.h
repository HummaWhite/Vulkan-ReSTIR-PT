#pragma once

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <optional>
#include <vector>

#include "Instance.h"
#include "util/NamespaceDecl.h"

NAMESPACE_BEGIN(zvk)

enum class QueueType {
    GeneralUse, AsyncCompute, AsyncTransfer, Present
};

struct Queue {
    Queue() : familyIdx(0), index(0) {}

    Queue(vk::Device device, uint32_t familyIdx, uint32_t index) : index(index), familyIdx(familyIdx) {
        queue = device.getQueue(familyIdx, index);
    }

    bool operator == (const Queue& rhs) const {
        return queue == rhs.queue &&
            familyIdx == rhs.familyIdx &&
            index == index;
    }

    bool operator != (const Queue& rhs) const {
        return !(*this == rhs);
    }

    vk::Queue queue;
    uint32_t familyIdx;
    uint32_t index;
};

class Context {
public:
    Context() = default;
    Context(const Instance& instance, const std::vector<const char*>& extensions);
    void destroy();

    Queue getQueue(QueueType type) const;

    static Context create(const Instance& instance, const std::vector<const char*>& extensions) {
        return Context(instance, extensions);
    }

public:
    vk::Device device;
    vk::PhysicalDeviceMemoryProperties memProperties;

    Queue queGeneralUse;
    Queue queAsyncCompute;
    Queue queAsyncTransfer;
    Queue quePresent;
};

NAMESPACE_END(zvk)