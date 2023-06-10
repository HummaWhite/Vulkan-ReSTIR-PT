#include "Descriptor.h"

NAMESPACE_BEGIN(zvk)

DescriptorSetLayout::DescriptorSetLayout(
    const Context* ctx, const std::vector<vk::DescriptorSetLayoutBinding>& bindings) :
    BaseVkObject(ctx), bindings(bindings)
{
    auto createInfo = vk::DescriptorSetLayoutCreateInfo()
        .setBindings(bindings);

    layout = mCtx->device.createDescriptorSetLayout(createInfo);

    for (const auto& binding : bindings) {
        types[binding.binding] = binding.descriptorType;
    }
}

DescriptorPool::DescriptorPool(
    const Context* ctx, const std::vector<DescriptorSetLayout>& layouts, uint32_t numCopies) :
    BaseVkObject(ctx)
{
    std::map<vk::DescriptorType, uint32_t> typeCount;

    for (const auto& layout : layouts) {
        for (const auto& binding : layout.bindings) {
            if (typeCount.find(binding.descriptorType) == typeCount.end()) {
                typeCount[binding.descriptorType] = 0;
            }
            typeCount[binding.descriptorType] += binding.descriptorCount;
        }
    }

    std::vector<vk::DescriptorPoolSize> sizes;

    for (const auto& count : typeCount) {
        sizes.push_back(vk::DescriptorPoolSize(count.first, count.second * numCopies));
    }

    auto createInfo = vk::DescriptorPoolCreateInfo()
        .setPoolSizes(sizes)
        .setMaxSets(layouts.size() * numCopies);

    pool = mCtx->device.createDescriptorPool(createInfo);
}

namespace Descriptor {
    vk::DescriptorSetLayoutBinding makeBinding(
        uint32_t binding, vk::DescriptorType type, vk::ShaderStageFlags stages,
        uint32_t count, const vk::Sampler* immutableSamplers
    ) {
        return vk::DescriptorSetLayoutBinding(binding, type, count, stages, immutableSamplers);
    }

    vk::WriteDescriptorSet makeWrite(
        const DescriptorSetLayout& layout,
        vk::DescriptorSet set, uint32_t binding, const vk::DescriptorBufferInfo& bufferInfo,
        uint32_t arrayElement
    ) {
        return vk::WriteDescriptorSet(
            set, binding, arrayElement, 1, layout.types.at(binding), nullptr, &bufferInfo
        );
    }

    vk::WriteDescriptorSet makeWrite(
        const DescriptorSetLayout& layout,
        vk::DescriptorSet set, uint32_t binding, const vk::DescriptorImageInfo& imageInfo,
        uint32_t arrayElement
    ) {
        return vk::WriteDescriptorSet(
            set, binding, arrayElement, 1, layout.types.at(binding), &imageInfo, nullptr
        );
    }
}

NAMESPACE_END(zvk)
