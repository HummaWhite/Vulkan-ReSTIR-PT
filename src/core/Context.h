#pragma once

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <optional>
#include <vector>
#include <array>

#include "Instance.h"
#include "util/NamespaceDecl.h"

NAMESPACE_BEGIN(zvk)

enum class QueueIdx {
    GeneralUse = 0, Present, AsyncCompute, AsyncTransfer, Ignored
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

template<typename T, size_t N, typename IdxT>
class ObjectSet {
public:
    T& operator [] (IdxT idx) {
        return mObjects[static_cast<size_t>(idx)];
    }

    const T& operator [] (IdxT idx) const {
        return mObjects[static_cast<size_t>(idx)];
    }

    std::array<T, N>& array() {
        return mObjects;
    }

    const std::array<T, N>& array() const {
        return mObjects;
    }

private:
    std::array<T, N> mObjects;
};

using QueueSet = ObjectSet<Queue, 4, QueueIdx>;
using CommandPoolSet = ObjectSet<vk::CommandPool, 4, QueueIdx>;

class Context {
public:
    Context() : mInstance(nullptr) {}
    Context(const Instance* instance, const std::vector<const char*>& extensions);
    ~Context() { destroy(); }
    void destroy();

    const Instance* instance() const { return mInstance; }

private:
    void createCmdPools();

public:
    vk::Device device;

    QueueSet queues;
    CommandPoolSet cmdPools;

private:
    const Instance* mInstance;
};

class BaseVkObject {
public:
    BaseVkObject() : mCtx(nullptr) {}
    BaseVkObject(const Context* ctx) : mCtx(ctx) {}
    BaseVkObject(const Context& ctx) : mCtx(&ctx) {}

protected:
    const Context* mCtx;
};

NAMESPACE_END(zvk)