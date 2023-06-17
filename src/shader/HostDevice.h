#ifndef HOST_DEVICE_H
#define HOST_DEVICE_H

#ifdef __cplusplus
  #include <vulkan/vulkan.hpp>

  #define ENUM_BEGIN(x) enum x {
  #define ENUM_END(x) }
  #define SWAPCHAIN_FORMAT vk::Format::eB8G8R8A8Unorm

  inline uint32_t ceilDiv(uint32_t x, uint32_t y) {
      return (x + y - 1) / y;
  }

#else
  #define ENUM_BEGIN(x) uint
  #define ENUM_END(x) ;
  #define SWAPCHAIN_FORMAT rgba8
  #define uint32_t uint
  #define glm::vec2 vec2
  #define glm::vec3 vec3
  #define glm::ivec2 ivec2
  #define glm::ivec3 ivec3

  #define InvalidResourceIdx -1
#endif

const int PostProcBlockSizeX = 32;
const int PostProcBlockSizeY = 32;

const int SceneResourceDescSet = 0;
const int SwapchainStorageDescSet = 1;

#endif