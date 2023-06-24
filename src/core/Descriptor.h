#pragma once

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <optional>
#include <vector>
#include <map>

#include "Context.h"
#include "Memory.h"

NAMESPACE_BEGIN(zvk)

class DescriptorSetLayout : public BaseVkObject {
public:
    DescriptorSetLayout(const Context* ctx, const std::vector<vk::DescriptorSetLayoutBinding>& bindings);
    ~DescriptorSetLayout() { destroy(); }

    void destroy() {
        mCtx->device.destroyDescriptorSetLayout(layout);
    }

public:
    vk::DescriptorSetLayout layout;
    std::map<uint32_t, vk::DescriptorType> types;
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
};

class DescriptorPool : public BaseVkObject {
public:
    DescriptorPool(
        const Context* ctx, const std::vector<DescriptorSetLayout*>& layouts, uint32_t numCopies,
        vk::DescriptorPoolCreateFlags flags = vk::DescriptorPoolCreateFlags{ 0 });

    ~DescriptorPool() { destroy(); }

    void destroy() {
        mCtx->device.destroyDescriptorPool(pool);
    }

    vk::DescriptorSet allocDescriptorSet(vk::DescriptorSetLayout layout) {
        return mCtx->device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo(pool, layout))[0];
    }

    std::vector<vk::DescriptorSet> allocDescriptorSets(vk::DescriptorSetLayout layout, uint32_t count) {
        std::vector<vk::DescriptorSetLayout> layouts(count, layout);
        return mCtx->device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo(pool, layouts));
    }

public:
    vk::DescriptorPool pool;
};

struct DescriptorWrite {
    void add(
        const DescriptorSetLayout* layout, vk::DescriptorSet set, uint32_t binding,
        const vk::DescriptorBufferInfo& bufferInfo);

    void add(
        const DescriptorSetLayout* layout, vk::DescriptorSet set, uint32_t binding,
        const vk::DescriptorImageInfo& imageInfo);

    void add(
        const DescriptorSetLayout* layout, vk::DescriptorSet set, uint32_t binding,
        const std::vector<vk::DescriptorImageInfo>& imageInfo);

    void add(
        const DescriptorSetLayout* layout, vk::DescriptorSet set, uint32_t binding,
        const vk::WriteDescriptorSetAccelerationStructureKHR& accel);

    std::vector<std::vector<vk::DescriptorImageInfo>> imageArrayInfo;
    std::vector<vk::WriteDescriptorSet> writes;
};

namespace Descriptor {
    vk::DescriptorSetLayoutBinding makeBinding(
        uint32_t binding, vk::DescriptorType type, vk::ShaderStageFlags stages,
        uint32_t count = 1, const vk::Sampler* immutableSamplers = nullptr);

    std::vector<vk::DescriptorImageInfo> makeImageDescriptorArray(const std::vector<Image*>& images);
}

NAMESPACE_END(zvk)