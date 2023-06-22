#include "AccelerationStructure.h"
#include "Command.h"

NAMESPACE_BEGIN(zvk)

AccelerationStructure::AccelerationStructure(
    const Context* ctx, QueueIdx queueIdx,
    const std::vector<AccelerationStructureTriangleMesh>& triangleMeshes,
    vk::BuildAccelerationStructureFlagsKHR flags
) : BaseVkObject(ctx), type(vk::AccelerationStructureTypeKHR::eBottomLevel)
{
    const auto& ext = mCtx->instance()->extFunctions();

    std::vector<vk::AccelerationStructureGeometryKHR> geometries;
    std::vector<uint32_t> maxPrimitiveCounts;
    std::vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRangeInfos;

    for (const auto& triangle : triangleMeshes) {
        auto triangleData = vk::AccelerationStructureGeometryTrianglesDataKHR()
            .setVertexData(triangle.vertexAddress)
            .setMaxVertex(triangle.numVertices)
            .setVertexFormat(triangle.vertexFormat)
            .setVertexStride(triangle.vertexStride)
            .setIndexData(triangle.indexAddress)
            .setIndexType(triangle.indexType);

        auto geometryData = vk::AccelerationStructureGeometryDataKHR()
            .setTriangles(triangleData);

        auto geometry = vk::AccelerationStructureGeometryKHR()
            .setGeometry(geometryData)
            .setGeometryType(vk::GeometryTypeKHR::eTriangles);

        auto buildRange = vk::AccelerationStructureBuildRangeInfoKHR()
            .setPrimitiveCount(triangle.numIndices / 3)
            .setPrimitiveOffset(0)
            .setFirstVertex(0)
            .setTransformOffset(0);

        geometries.push_back(geometry);
        maxPrimitiveCounts.push_back(triangle.numIndices / 3);
        buildRangeInfos.push_back(buildRange);
    }

    buildAccelerationStructure(geometries, maxPrimitiveCounts, buildRangeInfos, flags, queueIdx);
}

AccelerationStructure::AccelerationStructure(
    const Context* ctx, QueueIdx queueIdx,
    const AccelerationStructure* BLAS, vk::TransformMatrixKHR transform,
    vk::BuildAccelerationStructureFlagsKHR flags
) : BaseVkObject(ctx), type(vk::AccelerationStructureTypeKHR::eTopLevel)
{
    const auto& ext = mCtx->instance()->extFunctions();

    auto instance = vk::AccelerationStructureInstanceKHR()
        .setTransform(transform)
        .setInstanceCustomIndex(0)
        .setMask(0xff)
        .setInstanceShaderBindingTableRecordOffset(0)
        .setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable)
        .setAccelerationStructureReference(BLAS->address);

    auto instanceBuffer = Memory::createBuffer(
        mCtx, sizeof(vk::AccelerationStructureInstanceKHR),
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    auto instanceData = vk::AccelerationStructureGeometryInstancesDataKHR()
        .setArrayOfPointers(false)
        .setData(instanceBuffer->address());

    auto geometryData = vk::AccelerationStructureGeometryDataKHR()
        .setInstances(instanceData);

    auto geometry = vk::AccelerationStructureGeometryKHR()
        .setGeometryType(vk::GeometryTypeKHR::eInstances)
        .setFlags(vk::GeometryFlagBitsKHR::eOpaque)
        .setGeometry(geometryData);

    uint32_t maxPrimitiveCount = 1;

    auto buildRangeInfo = vk::AccelerationStructureBuildRangeInfoKHR()
        .setPrimitiveCount(1)
        .setPrimitiveOffset(0)
        .setFirstVertex(0)
        .setTransformOffset(0);

    buildAccelerationStructure(geometry, 1, buildRangeInfo, flags, queueIdx);
    delete instanceBuffer;
}

void AccelerationStructure::buildAccelerationStructure(
    const vk::ArrayProxy<const vk::AccelerationStructureGeometryKHR>& geometries,
    const vk::ArrayProxy<const uint32_t>& maxPrimitiveCounts,
    const vk::ArrayProxy<const vk::AccelerationStructureBuildRangeInfoKHR>& buildRangeInfos,
    vk::BuildAccelerationStructureFlagsKHR flags,
    QueueIdx queueIdx
) {
    const auto& ext = mCtx->instance()->extFunctions();

    auto buildGeometryInfo = vk::AccelerationStructureBuildGeometryInfoKHR()
        .setType(type)
        .setFlags(flags)
        .setGeometries(geometries);

    auto buildSizeInfo = ext.getAccelerationStructureBuildSizesKHR(
        mCtx->device, vk::AccelerationStructureBuildTypeKHR::eDevice, buildGeometryInfo, maxPrimitiveCounts
    );

    mBuffer = Memory::createBuffer(
        mCtx, buildSizeInfo.accelerationStructureSize,
        vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        vk::MemoryPropertyFlagBits::eDeviceLocal, vk::MemoryAllocateFlagBits::eDeviceAddress
    );

    auto createInfo = vk::AccelerationStructureCreateInfoKHR()
        .setBuffer(mBuffer->buffer)
        .setSize(buildSizeInfo.accelerationStructureSize)
        .setType(type);

    structure = ext.createAccelerationStructureKHR(mCtx->device, createInfo);

    auto addressInfo = vk::AccelerationStructureDeviceAddressInfoKHR()
        .setAccelerationStructure(structure);

    address = ext.getAccelerationStructureDeviceAddressKHR(mCtx->device, addressInfo);

    auto scratchBuffer = Memory::createBuffer(
        mCtx, buildSizeInfo.buildScratchSize,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        vk::MemoryPropertyFlagBits::eDeviceLocal, vk::MemoryAllocateFlagBits::eDeviceAddress
    );

    buildGeometryInfo
        .setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
        .setDstAccelerationStructure(structure)
        .setScratchData(scratchBuffer->address());

    auto cmd = Command::createOneTimeSubmit(mCtx, queueIdx);
    ext.cmdBuildAccelerationStructuresKHR(cmd->cmd, buildGeometryInfo, { buildRangeInfos.data() });
    cmd->oneTimeSubmit();

    delete cmd;
    delete scratchBuffer;
}

NAMESPACE_END(zvk)
