#pragma once

#include <vulkan/vulkan.hpp>

#include "util/NamespaceDecl.h"

NAMESPACE_BEGIN(zvk)

struct Context {
    vk::Device device;
    vk::PhysicalDevice physicalDevice;
    vk::CommandPool commandPool;
};

NAMESPACE_END(zvk)