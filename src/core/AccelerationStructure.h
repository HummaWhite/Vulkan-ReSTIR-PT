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
    uint32_t numVertices;
    uint32_t numIndices;
};

class AccelerationStructure : public BaseVkObject {
public:
    AccelerationStructure(
        const Context* ctx, QueueIdx queueIdx,
        const std::vector<AccelerationStructureTriangleMesh>& triangleMeshes,
        vk::BuildAccelerationStructureFlagsKHR flags);

    AccelerationStructure(
        const Context* ctx, QueueIdx queueIdx,
        const AccelerationStructure* BLAS, vk::TransformMatrixKHR transform,
        vk::BuildAccelerationStructureFlagsKHR flags);

    ~AccelerationStructure() { destroy(); }

    void destroy() {
        mCtx->instance()->extFunctions().destroyAccelerationStructureKHR(mCtx->device, structure);
        delete mBuffer;
    }

private:
    void buildAccelerationStructure(
        const vk::ArrayProxy<const vk::AccelerationStructureGeometryKHR>& geometries,
        const vk::ArrayProxy<const uint32_t>& maxPrimitiveCounts,
        const vk::ArrayProxy<const vk::AccelerationStructureBuildRangeInfoKHR>& buildRangeInfos,
        vk::BuildAccelerationStructureFlagsKHR flags,
        QueueIdx queueIdx);

public:
    vk::AccelerationStructureKHR structure;
    vk::AccelerationStructureTypeKHR type;
    vk::DeviceAddress address;

private:
    Buffer* mBuffer = nullptr;
};

NAMESPACE_END(zvk)