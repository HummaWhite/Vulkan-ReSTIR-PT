#include "Renderer.h"

VkResult createInstance(
    const std::vector<const char*>& extensionNames,
    const std::vector<const char*>& layerNames,
    const void* featureChain,
    VkInstance& instance
) {
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Application",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = featureChain,
        .flags = VkInstanceCreateFlags(0),
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(layerNames.size()),
        .ppEnabledLayerNames = layerNames.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensionNames.size()),
        .ppEnabledExtensionNames = extensionNames.data(),
    };

    return vkCreateInstance(&createInfo, nullptr, &instance);
}

vk::Instance createInstanceCpp(
    const std::vector<const char*>& extensionNames,
    const std::vector<const char*>& layerNames,
    const void* featureChain
) {
    auto appInfo = vk::ApplicationInfo()
        .setPApplicationName("Application")
        .setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
        .setPEngineName("Engine")
        .setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
        .setApiVersion(VK_API_VERSION_1_0);

    auto createInfo = vk::InstanceCreateInfo()
        .setPApplicationInfo(&appInfo)
        .setPEnabledLayerNames(layerNames)
        .setPEnabledExtensionNames(extensionNames)
        .setPNext(featureChain);

    return vk::createInstance(createInfo);
}

VkResult createLogicalDevice2(
    VkPhysicalDevice physicalDevice,
    const std::vector<const char*>& layerNames,
    const std::vector<const char*>& extensionNames,
    const VkPhysicalDeviceFeatures& enabledFeatures,
    const std::vector<VkDeviceQueueCreateInfo>& queueCreateInfos,
    VkDevice& device
) {
    VkDeviceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VkDeviceCreateFlags(0),
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = static_cast<uint32_t>(layerNames.size()),
        .ppEnabledLayerNames = layerNames.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensionNames.size()),
        .ppEnabledExtensionNames = extensionNames.data(),
        .pEnabledFeatures = &enabledFeatures
    };

    return vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
}

VkResult createLogicalDevice2(
    VkPhysicalDevice physicalDevice,
    const std::vector<const char*>& layerNames,
    const std::vector<const char*>& extensionNames,
    const VkPhysicalDeviceFeatures& enabledFeatures,
    void* extensionFeatureChain,
    const std::vector<VkDeviceQueueCreateInfo>& queueCreateInfos,
    VkDevice& device
) {
    VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = extensionFeatureChain,
        .features = enabledFeatures
    };

    VkDeviceCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &physicalDeviceFeatures2,
        .flags = VkDeviceCreateFlags(0),
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = static_cast<uint32_t>(layerNames.size()),
        .ppEnabledLayerNames = layerNames.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensionNames.size()),
        .ppEnabledExtensionNames = extensionNames.data(),
    };

    return vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
}

VkResult createBuffer() {
    struct Vertex {};
    std::vector<VkDrawIndexedIndirectCommand> vertices;
    VkBuffer indirectDrawBuffer;
    VkDevice device = 0;

    VkBufferCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = sizeof(Vertex) * vertices.size(),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    if (vkCreateBuffer(device, &createInfo, nullptr, &indirectDrawBuffer) != VkResult::VK_SUCCESS) {
        throw std::runtime_error("Fail to create buffer");
    }

    VkBufferView bufferView;

    VkBufferViewCreateInfo viewCreateInfo {
    };
}

VkResult createExample2DImage(
    VkDevice device,
    uint32_t width, uint32_t height,
    VkImage& image
) {
    VkImageCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VkImageCreateFlags{ 0 },
        .format = VK_FORMAT_R16G16B16A16_UINT,
        .extent = VkExtent3D{ width, height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_STORAGE_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    return vkCreateImage(device, &createInfo, nullptr, &image);
}

std::optional<uint32_t> findMemoryTypeIndex(
    const VkPhysicalDeviceMemoryProperties& physicalDeviceMemProps,
    const VkMemoryRequirements& requirements,
    VkMemoryPropertyFlags requestedProps
) {
    for (uint32_t i = 0; i < physicalDeviceMemProps.memoryTypeCount; i++) {
        if ((requirements.memoryTypeBits & (1 << i) &&
            ((requestedProps & physicalDeviceMemProps.memoryTypes[i].propertyFlags) == requestedProps))) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<VkDeviceMemory> allocateAndBindBufferMemory(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkBuffer buffer, VkDeviceSize size,
    VkImage image,
    VkMemoryPropertyFlags memoryPropFlags,
    VkMemoryAllocateFlags allocFlags
) {
    VkDeviceMemory memory;

    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProps;
    VkMemoryRequirements requirements;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProps);
    vkGetBufferMemoryRequirements(device, buffer, &requirements);
    // vkGetImageMemoryRequirements(device, image, &requirements);

    auto memoryTypeIndex = findMemoryTypeIndex(physicalDeviceMemoryProps, requirements, memoryPropFlags);

    if (!memoryTypeIndex.has_value()) {
        throw std::runtime_error("No memory type supported");
    }
    VkMemoryAllocateFlagsInfo allocFlagsInfo {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = allocFlags
    };

    VkMemoryAllocateInfo allocInfo {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = (allocFlags == 0) ? nullptr : &allocFlagsInfo,
        .allocationSize = requirements.size,
        .memoryTypeIndex = memoryTypeIndex.value()
    };
    vkAllocateMemory(device, &allocInfo, nullptr, &memory);
}

void getQueue(vk::Device device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue& queue) {
    vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &queue);
}

#include <gl/GL.h>

void descriptors() {
    VkBuffer camera;

    VkBuffer vertices;
    VkBuffer indices;
    VkBuffer materials = 0;
    std::vector<VkImage> textures;

    VkAccelerationStructureKHR accelStructure;

    VkImage rayOutput;
    
    VkDevice device = 0;

    VkDescriptorSetLayout descriptorSetLayout{};

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS },
        { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS },
        { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS },
        { 3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100, VK_SHADER_STAGE_ALL_GRAPHICS },
    };
    /*
    typedef struct VkWriteDescriptorSet {
    VkStructureType                  sType;
    const void*                      pNext;
    VkDescriptorSet                  dstSet;
    uint32_t                         dstBinding;
    uint32_t                         dstArrayElement;
    uint32_t                         descriptorCount;
    VkDescriptorType                 descriptorType;
    const VkDescriptorImageInfo*     pImageInfo;
    const VkDescriptorBufferInfo*    pBufferInfo;
    const VkBufferView*              pTexelBufferView;
} VkWriteDescriptorSet;
    */

    const VkDeviceSize bufferSize = 128;

    VkDescriptorSet descriptorSet{};
    VkDescriptorSetLayout descriptorSetLayout{};
    VkImageView imageView{};
    VkImageLayout imageLayout{};
    VkSampler sampler{};

    VkWriteDescriptorSet writes[2];

    VkDescriptorBufferInfo bufferInfo {
        .buffer = vertices, .offset = 0, .range = bufferSize
    };
    writes[0] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSet, .dstBinding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &bufferInfo
    };

    VkDescriptorImageInfo imageInfo[100] = {
        { .sampler = sampler, .imageView = imageView, .imageLayout = imageLayout },
        // more image info ...
    };
    writes[1] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSet, .dstBinding = 3,
        .dstArrayElement = 0, .descriptorCount = 100,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = imageInfo
    };

    vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);

    struct Descriptor {
        VkDescriptorType type;
        union {
            VkBuffer buffer;
            VkImage image;
        };
        uint32_t shaderBindingSlot;
        uint32_t inSlotOffset;
    };

    struct DescriptorSetLayout {
        Descriptor binding0;
        Descriptor binding1;
        Descriptor binding2;
        Descriptor binding3[100];
    };

    DescriptorSetLayout set0;
    set0.binding0 = { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .buffer = materials };
    set0.binding1 = { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .buffer = vertices };
    set0.binding2 = { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .buffer = indices };
    set0.binding3[27] = { .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .image = textures[27] };
}

VkResult createShaderModule(
    VkDevice device,
    const uint32_t* source, uint32_t sourceSize,
    VkShaderModule& shaderModule
) {
    VkShaderModuleCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sourceSize,
        .pCode = source,
    };
    return vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
}

VkResult createPipeline(
) {
    VkDescriptorSetLayout cameraUniformDescSetLayout{};
    VkDescriptorSetLayout geometryDescSetLayout{};

    /*
    typedef struct VkPipelineLayoutCreateInfo {
    VkStructureType                 sType;
    const void*                     pNext;
    VkPipelineLayoutCreateFlags     flags;
    uint32_t                        setLayoutCount;
    const VkDescriptorSetLayout*    pSetLayouts;
    uint32_t                        pushConstantRangeCount;
    const VkPushConstantRange*      pPushConstantRanges;
} VkPipelineLayoutCreateInfo;
    */
    VkDevice device{};
    VkPipelineLayout pipelineLayout;

    struct PushConstantStruct {} pushConstantData;
    struct Camera {} cameraPushConstant;

    VkPipeline pipeline;

    std::array<VkDescriptorSetLayout, 2> descSetLayouts = { cameraUniformDescSetLayout, geometryDescSetLayout };

    VkPushConstantRange pushConstantRange {
        .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
        .offset = 0, .size = static_cast<uint32_t>(sizeof(PushConstantStruct))
    };

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(descSetLayouts.size()),
        .pSetLayouts = descSetLayouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };
    vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);

    VkCommandBuffer commandBuffer{};


    vkCmdPushConstants(
        /* VkCommandBuffer    */ commandBuffer,
        /* VkPipelineLayout   */ pipelineLayout,
        /* VkShaderStageFlags */ VK_SHADER_STAGE_ALL_GRAPHICS,
        /* uint32_t offset    */ 0,
        /* uint32_t range     */ static_cast<uint32_t>(sizeof(Camera)),
        /* const void* data   */ &cameraPushConstant
    );
}