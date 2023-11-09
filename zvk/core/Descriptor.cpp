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
    const Context* ctx, const vk::ArrayProxy<DescriptorSetLayout*>& layouts, uint32_t numCopies,
    vk::DescriptorPoolCreateFlags flags
) : BaseVkObject(ctx)
{
    std::map<vk::DescriptorType, uint32_t> typeCount;

    for (const auto& layout : layouts) {
        for (const auto& binding : layout->bindings) {
            if (typeCount.find(binding.descriptorType) == typeCount.end()) {
                typeCount[binding.descriptorType] = 0;
            }
            typeCount[binding.descriptorType] += binding.descriptorCount;
        }
    }

    std::vector<vk::DescriptorPoolSize> sizes;

    for (const auto& count : typeCount) {
        sizes.push_back(vk::DescriptorPoolSize(count.first, count.second));
    }

    auto createInfo = vk::DescriptorPoolCreateInfo()
        .setPoolSizes(sizes)
        .setFlags(flags)
        .setMaxSets(layouts.size() * numCopies);

    pool = mCtx->device.createDescriptorPool(createInfo);
}

DescriptorPool::DescriptorPool(
    const Context* ctx, const vk::ArrayProxy<vk::DescriptorPoolSize>& sizes,
    vk::DescriptorPoolCreateFlags flags
) : BaseVkObject(ctx)
{
    auto createInfo = vk::DescriptorPoolCreateInfo()
        .setPoolSizes(sizes)
        .setFlags(flags)
        .setMaxSets(1);

    pool = mCtx->device.createDescriptorPool(createInfo);
}

void DescriptorWrite::add(
    const DescriptorSetLayout* layout,
    vk::DescriptorSet set, uint32_t binding, const vk::DescriptorBufferInfo& bufferInfo
) {
    writes.push_back({ set, binding, 0, 1, layout->types.at(binding), nullptr, &bufferInfo });
}

void DescriptorWrite::add(
    const DescriptorSetLayout* layout,
    vk::DescriptorSet set, uint32_t binding, const vk::DescriptorImageInfo& imageInfo
) {
    writes.push_back({ set, binding, 0, 1, layout->types.at(binding), &imageInfo, nullptr });
}

void DescriptorWrite::add(
    const DescriptorSetLayout* layout, vk::DescriptorSet set, uint32_t binding,
    const std::vector<vk::DescriptorImageInfo>& imageInfo
) {
    mImageArrayInfo.push_back(imageInfo);

    writes.push_back(
        vk::WriteDescriptorSet(
            set, binding, 0, static_cast<uint32_t>(imageInfo.size()), layout->types.at(binding),
            mImageArrayInfo[mImageArrayInfo.size() - 1].data(), nullptr, nullptr
        )
    );
}

void DescriptorWrite::add(
    const DescriptorSetLayout* layout, vk::DescriptorSet set, uint32_t binding,
    const vk::WriteDescriptorSetAccelerationStructureKHR& accelInfo
) {
    writes.push_back({ set, binding, 0, 1, layout->types.at(binding), nullptr, nullptr, nullptr, &accelInfo });
}

void DescriptorWrite::flush() {
    mCtx->device.updateDescriptorSets(writes, {});
    writes.clear();
    mImageArrayInfo.clear();
}

namespace Descriptor {
    vk::DescriptorSetLayoutBinding makeBinding(
        uint32_t binding, vk::DescriptorType type, vk::ShaderStageFlags stages,
        uint32_t count, const vk::Sampler* immutableSamplers
    ) {
        return vk::DescriptorSetLayoutBinding(binding, type, count, stages, immutableSamplers);
    }

    vk::DescriptorBufferInfo makeBufferInfo(const zvk::Buffer* buffer) {
        return vk::DescriptorBufferInfo(buffer->buffer, 0, buffer->size);
    }

    vk::DescriptorImageInfo makeImageInfo(const zvk::Image* image) {
        return vk::DescriptorImageInfo(image->sampler, image->view, image->layout);
    }

    std::vector<vk::DescriptorImageInfo> makeImageDescriptorArray(const std::vector<Image*>& images) {
        std::vector<vk::DescriptorImageInfo> info;

        for (auto image : images) {
            info.push_back({ image->sampler, image->view, image->layout });
        }
        return info;
    }

    std::vector<vk::DescriptorImageInfo> makeImageDescriptorArray(const std::vector<std::unique_ptr<Image>>& images) {
        std::vector<vk::DescriptorImageInfo> info;

        for (const auto& image : images) {
            info.push_back({ image->sampler, image->view, image->layout });
        }
        return info;
    }
}

NAMESPACE_END(zvk)
