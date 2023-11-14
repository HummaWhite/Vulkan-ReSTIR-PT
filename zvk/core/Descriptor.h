#pragma once

#include <vulkan/vulkan.hpp>
#include <iostream>
#include <optional>
#include <vector>
#include <map>
#include <unordered_set>

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

using DescriptorLayoutArray = vk::ArrayProxy<DescriptorSetLayout*>;

class DescriptorPool : public BaseVkObject {
public:
    DescriptorPool(
        const Context* ctx, const vk::ArrayProxy<DescriptorSetLayout*>& layouts, uint32_t numCopies,
        vk::DescriptorPoolCreateFlags flags = vk::DescriptorPoolCreateFlags{ 0 });

    DescriptorPool(
        const Context* ctx, const vk::ArrayProxy<vk::DescriptorPoolSize>& sizes,
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

class DescriptorWrite : public BaseVkObject {
public:
    DescriptorWrite(const Context* ctx) : BaseVkObject(ctx) {}

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

    void flush();

public:
    std::vector<vk::WriteDescriptorSet> writes;

private:
    std::vector<std::vector<vk::DescriptorImageInfo>> mImageArrayInfo;
};

namespace Descriptor {
    vk::DescriptorSetLayoutBinding makeBinding(
        uint32_t binding, vk::DescriptorType type, vk::ShaderStageFlags stages,
        uint32_t count = 1, const vk::Sampler* immutableSamplers = nullptr);

    vk::DescriptorBufferInfo makeBuffer(const zvk::Buffer* buffer);
    vk::DescriptorImageInfo makeImage(const zvk::Image* image);

    std::vector<vk::DescriptorImageInfo> makeImageArray(const std::vector<Image*>& images);
    std::vector<vk::DescriptorImageInfo> makeImageArray(const std::vector<std::unique_ptr<Image>>& images);
}

NAMESPACE_END(zvk)