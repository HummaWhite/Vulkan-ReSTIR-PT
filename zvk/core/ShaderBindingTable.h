#pragma once

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <optional>
#include <vector>

#include "Context.h"
#include "Memory.h"

NAMESPACE_BEGIN(zvk)

class ShaderBindingTable : public BaseVkObject {
public:
    ShaderBindingTable(const Context* ctx, uint32_t numMissGroup, uint32_t numHitGroup, vk::Pipeline pipeline);

public:
    std::unique_ptr<Buffer> buffer;

    vk::StridedDeviceAddressRegionKHR rayGenRegion;
    vk::StridedDeviceAddressRegionKHR missRegion;
    vk::StridedDeviceAddressRegionKHR hitRegion;
    vk::StridedDeviceAddressRegionKHR callableRegion;
};

NAMESPACE_END(zvk)