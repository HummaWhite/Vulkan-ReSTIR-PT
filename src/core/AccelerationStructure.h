#pragma once

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <optional>
#include <vector>

#include "Context.h"
#include "Memory.h"

NAMESPACE_BEGIN(zvk)

struct AccelerationStructureTriangleMesh {
    vk::DeviceAddress vertexAddress;
    vk::DeviceAddress indexAddress;
    vk::DeviceSize vertexStride;
    vk::Format vertexFormat;
    vk::IndexType indexType;
    uint32_t maxVertex;
    uint32_t numIndices;
    uint32_t indexOffset;
};

class AccelerationStructure : public BaseVkObject {
public:
    AccelerationStructure(
        const Context* ctx, QueueIdx queueIdx,
        const vk::ArrayProxy<const AccelerationStructureTriangleMesh>& triangleMeshes,
        vk::BuildAccelerationStructureFlagsKHR flags);

    AccelerationStructure(
        const Context* ctx, QueueIdx queueIdx,
        const vk::ArrayProxy<const vk::AccelerationStructureInstanceKHR>& instances,
        vk::BuildAccelerationStructureFlagsKHR flags);

    ~AccelerationStructure() { destroy(); }

    void destroy() {
        mCtx->instance()->extFunctions().destroyAccelerationStructureKHR(mCtx->device, structure);
        mBuffer.reset();
    }

private:
    void buildAccelerationStructure(
        QueueIdx queueIdx,
        const vk::ArrayProxy<const vk::AccelerationStructureGeometryKHR>& geometries,
        const vk::ArrayProxy<const vk::AccelerationStructureBuildRangeInfoKHR>& buildRangeInfos,
        vk::BuildAccelerationStructureFlagsKHR flags);

public:
    vk::AccelerationStructureKHR structure;
    vk::AccelerationStructureTypeKHR type;
    vk::DeviceAddress address;

private:
    std::unique_ptr<Buffer> mBuffer;
};

NAMESPACE_END(zvk)