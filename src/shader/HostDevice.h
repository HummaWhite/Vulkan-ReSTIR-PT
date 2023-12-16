#ifndef HOST_DEVICE_H
#define HOST_DEVICE_H

#ifdef __cplusplus
  #include <vulkan/vulkan.hpp>

  constexpr uint32_t NumFramesInFlight = 1;

  #define ENUM_BEGIN(x) enum x {
  #define ENUM_END(x) }
  #define SWAPCHAIN_FORMAT vk::Format::eB8G8R8A8Unorm

const vk::ShaderStageFlags RayPipelineShaderStageFlags =
    vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eAnyHitKHR |
    vk::ShaderStageFlagBits::eMissKHR | vk::ShaderStageFlagBits::eClosestHitKHR;

const vk::ShaderStageFlags RayQueryShaderStageFlags =
    vk::ShaderStageFlagBits::eCompute;

#else
  #define ENUM_BEGIN(x) uint
  #define ENUM_END(x) ;
  #define SWAPCHAIN_FORMAT rgba8

  #define int32_t int
  #define uint32_t uint
#endif

#define RESTIR_PT_MATERIAL 1

const uint32_t InvalidResourceIdx = 0xffffffff;

const uint32_t PostProcBlockSizeX = 32;
const uint32_t PostProcBlockSizeY = 32;

const uint32_t RayQueryBlockSizeX = 8;
const uint32_t RayQueryBlockSizeY = 8;

const uint32_t CameraDescSet = 0;
const uint32_t ResourceDescSet = 1;
const uint32_t RayImageDescSet = 2;
const uint32_t RayTracingDescSet = 3;
const uint32_t SwapchainStorageDescSet = 4;

#endif