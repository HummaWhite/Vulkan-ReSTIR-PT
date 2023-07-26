#include "AccelerationStructure.h"
#include "Command.h"

NAMESPACE_BEGIN(zvk)

AccelerationStructure::AccelerationStructure(
    const Context* ctx, QueueIdx queueIdx,
    const vk::ArrayProxy<const AccelerationStructureTriangleMesh>& triangleMeshes,
    vk::BuildAccelerationStructureFlagsKHR flags
) : BaseVkObject(ctx), type(vk::AccelerationStructureTypeKHR::eBottomLevel)
{
    const auto& ext = mCtx->instance()->extFunctions();

    std::vector<vk::AccelerationStructureGeometryKHR> geometries;
    std::vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRangeInfos;

    for (const auto& mesh : triangleMeshes) {
        auto triangleData = vk::AccelerationStructureGeometryTrianglesDataKHR()
            .setVertexData(mesh.vertexAddress)
            .setMaxVertex(mesh.maxVertex)
            .setVertexFormat(mesh.vertexFormat)
            .setVertexStride(mesh.vertexStride)
            .setIndexData(mesh.indexAddress)
            .setIndexType(mesh.indexType);

        auto geometryData = vk::AccelerationStructureGeometryDataKHR()
            .setTriangles(triangleData);

        auto geometry = vk::AccelerationStructureGeometryKHR()
            .setGeometry(geometryData)
            .setGeometryType(vk::GeometryTypeKHR::eTriangles);

        auto buildRange = vk::AccelerationStructureBuildRangeInfoKHR()
            .setPrimitiveCount(mesh.numIndices / 3)
            .setPrimitiveOffset(0)
            .setFirstVertex(mesh.indexOffset)
            .setTransformOffset(0);

        geometries.push_back(geometry);
        buildRangeInfos.push_back(buildRange);
    }

    buildAccelerationStructure(queueIdx, geometries, buildRangeInfos, flags);
}

AccelerationStructure::AccelerationStructure(
    const Context* ctx, QueueIdx queueIdx,
    const vk::ArrayProxy<const vk::AccelerationStructureInstanceKHR>& instances,
    vk::BuildAccelerationStructureFlagsKHR flags
) : BaseVkObject(ctx), type(vk::AccelerationStructureTypeKHR::eTopLevel)
{
    const auto& ext = mCtx->instance()->extFunctions();

    auto instanceBuffer = Memory::createBufferFromHost(
        mCtx, queueIdx, instances,
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        vk::MemoryAllocateFlagBits::eDeviceAddress
    );

    auto instanceData = vk::AccelerationStructureGeometryInstancesDataKHR()
        .setArrayOfPointers(false)
        .setData(instanceBuffer->address());

    auto geometryData = vk::AccelerationStructureGeometryDataKHR()
        .setInstances(instanceData);

    auto geometry = vk::AccelerationStructureGeometryKHR()
        .setGeometryType(vk::GeometryTypeKHR::eInstances)
        .setFlags(vk::GeometryFlagBitsKHR::eOpaque)
        .setGeometry(geometryData);

    auto buildRangeInfo = vk::AccelerationStructureBuildRangeInfoKHR()
        .setPrimitiveCount(1)
        .setPrimitiveOffset(0)
        .setFirstVertex(0)
        .setTransformOffset(0);

    buildAccelerationStructure(queueIdx, geometry, buildRangeInfo, flags);
    delete instanceBuffer;
}

void AccelerationStructure::buildAccelerationStructure(
    QueueIdx queueIdx,
    const vk::ArrayProxy<const vk::AccelerationStructureGeometryKHR>& geometries,
    const vk::ArrayProxy<const vk::AccelerationStructureBuildRangeInfoKHR>& buildRangeInfos,
    vk::BuildAccelerationStructureFlagsKHR flags
) {
    const auto& ext = mCtx->instance()->extFunctions();

    auto buildGeometryInfo = vk::AccelerationStructureBuildGeometryInfoKHR()
        .setType(type)
        .setFlags(flags)
        .setGeometries(geometries);

    std::vector<uint32_t> maxPrimitiveCounts;

    for (const auto& buildRange : buildRangeInfos) {
        maxPrimitiveCounts.push_back(buildRange.primitiveCount);
    }

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
    ext.cmdBuildAccelerationStructuresKHR(cmd->cmd, buildGeometryInfo, buildRangeInfos.data());
    cmd->submitAndWait();

    delete cmd;
    delete scratchBuffer;
}

NAMESPACE_END(zvk)
