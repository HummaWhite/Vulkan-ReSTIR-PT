#ifndef HOST_DEVICE_H
#define HOST_DEVICE_H

#ifdef __cplusplus
  #include <vulkan/vulkan.hpp>

  #define ENUM_BEGIN(x) enum x {
  #define ENUM_END(x) }
  #define SWAPCHAIN_FORMAT vk::Format::eB8G8R8A8Unorm

const vk::ShaderStageFlags RayTracingShaderStageFlags =
    vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eAnyHitKHR |
    vk::ShaderStageFlagBits::eMissKHR | vk::ShaderStageFlagBits::eClosestHitKHR;

namespace zvk {
    inline uint32_t ceilDiv(uint32_t x, uint32_t y) {
        return (x + y - 1) / y;
    }

    inline uint32_t align(uint32_t size, uint32_t alignment) {
        return ceilDiv(size, alignment) * alignment;
    }
}

#else
  #define ENUM_BEGIN(x) uint
  #define ENUM_END(x) ;
  #define SWAPCHAIN_FORMAT rgba8

  #define InvalidResourceIdx -1
#endif

const int PostProcBlockSizeX = 32;
const int PostProcBlockSizeY = 32;

const int ResourceDescSet = 0;
const int ImageOutputDescSet = 1;
const int RayTracingDescSet = 2;
const int SwapchainStorageDescSet = 3;

#endif