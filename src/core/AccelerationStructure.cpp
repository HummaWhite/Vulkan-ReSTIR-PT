#include "AccelerationStructure.h"
#include "Command.h"

NAMESPACE_BEGIN(zvk)

AccelerationStructure::AccelerationStructure(
    const Context* ctx, QueueIdx queueIdx,
    const std::vector<AccelerationStructureTriangleData>& triangleMeshes, vk::AccelerationStructureTypeKHR type,
    vk::BuildAccelerationStructureFlagsKHR flags
) : BaseVkObject(ctx), type(type)
{
    std::vector<vk::AccelerationStructureGeometryKHR> geometries;
    std::vector<uint32_t> maxPrimitiveCounts;

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

        geometries.push_back(geometry);
        maxPrimitiveCounts.push_back(triangle.numIndices / 3);
    }

    auto buildGeometryInfo = vk::AccelerationStructureBuildGeometryInfoKHR()
        .setType(type)
        .setFlags(flags)
        .setGeometries(geometries);

    auto buildSizeInfo = mCtx->device.getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice, buildGeometryInfo, maxPrimitiveCounts
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

    structure = mCtx->device.createAccelerationStructureKHR(createInfo);

    auto addressInfo = vk::AccelerationStructureDeviceAddressInfoKHR()
        .setAccelerationStructure(structure);

    address = mCtx->device.getAccelerationStructureAddressKHR(addressInfo);

    auto scratchBuffer = Memory::createBuffer(
        mCtx, buildSizeInfo.buildScratchSize,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        vk::MemoryPropertyFlagBits::eDeviceLocal, vk::MemoryAllocateFlagBits::eDeviceAddress
    );

    auto buildGeometryInfo = vk::AccelerationStructureBuildGeometryInfoKHR()
        .setType(type)
        .setFlags(flags)
        .setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
        .setDstAccelerationStructure(structure)
        .setGeometries(geometries)
        .setScratchData(scratchBuffer->address());

    std::vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRangeInfos;

    for (const auto& triangle : triangleMeshes) {
        auto buildRange = vk::AccelerationStructureBuildRangeInfoKHR()
            .setPrimitiveCount(triangle.numIndices / 3)
            .setPrimitiveOffset(0)
            .setFirstVertex(0)
            .setTransformOffset(0);

        buildRangeInfos.push_back(buildRange);
    }
    auto cmd = Command::createOneTimeSubmit(mCtx, queueIdx);

    cmd->cmd.buildAccelerationStructuresKHR(buildGeometryInfo, { buildRangeInfos.data() });

    cmd->oneTimeSubmit();
    delete cmd;
    delete scratchBuffer;
}

NAMESPACE_END(zvk)
