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

    inline uint32_t ceil(uint32_t x, uint32_t y) {
        return ceilDiv(x, y) * y;
    }
}

#else
  #define ENUM_BEGIN(x) uint
  #define ENUM_END(x) ;
  #define SWAPCHAIN_FORMAT rgba8

  #define InvalidResourceIdx -1
#endif

const int MaxNumTextures = 32;

const int PostProcBlockSizeX = 32;
const int PostProcBlockSizeY = 32;

const int CameraDescSet = 0;
const int ResourceDescSet = 1;
const int GBufferDrawParamDescSet = 2;
const int ImageOutputDescSet = 3;
const int RayTracingDescSet = 4;
const int SwapchainStorageDescSet = 5;

#endif