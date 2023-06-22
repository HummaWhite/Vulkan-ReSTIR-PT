#pragma once

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <optional>
#include <vector>

#include "Context.h"
#include "Memory.h"

NAMESPACE_BEGIN(zvk)

struct AccelerationStructureTriangleData {
    vk::DeviceAddress vertexAddress;
    vk::DeviceAddress indexAddress;
    vk::DeviceSize vertexStride;
    vk::Format vertexFormat;
    vk::IndexType indexType;
    uint32_t numVertices;
    uint32_t numIndices;
};

class AccelerationStructure : public BaseVkObject {
public:
    AccelerationStructure(
        const Context* ctx, QueueIdx queueIdx,
        const std::vector<AccelerationStructureTriangleData>& triangleMeshes, vk::AccelerationStructureTypeKHR type,
        vk::BuildAccelerationStructureFlagsKHR flags);

    ~AccelerationStructure() { destroy(); }

    void destroy() {
        mCtx->instance()->extFunctions().destroyAccelerationStructureKHR(mCtx->device, structure);
        delete mBuffer;
    }

public:
    vk::AccelerationStructureKHR structure;
    vk::AccelerationStructureTypeKHR type;
    vk::DeviceAddress address;

private:
    Buffer* mBuffer = nullptr;
};

NAMESPACE_END(zvk)