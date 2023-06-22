#pragma once

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <optional>
#include <vector>

#include "Context.h"
#include "Memory.h"

NAMESPACE_BEGIN(zvk)

class AccelerationStructure : public BaseVkObject {
public:
    AccelerationStructure(const Context* ctx, const zvk::Buffer* vertexBuffer, const zvk::Buffer* indexBuffer, QueueIdx queueIdx) : BaseVkObject(ctx) {}
    ~AccelerationStructure() { destroy(); }

    void destroy() {
        mCtx->device.destroyAccelerationStructureKHR(structure);
        delete mBuffer;
    }


private:
    void createBottomLevelStructure();
    void createTopLevelStructure();
    void createDescriptor();

public:
    vk::AccelerationStructureKHR structure;
    vk::DeviceAddress address;

private:
    Buffer* mBuffer = nullptr;
};

NAMESPACE_END(zvk)