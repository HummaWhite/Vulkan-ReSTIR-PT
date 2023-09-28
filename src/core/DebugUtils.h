#pragma once

#include <vulkan/vulkan.hpp>
#include <array>

#include "util/NamespaceDecl.h"
#include "core/ExtFunctions.h"

NAMESPACE_BEGIN(zvk::DebugUtils)

template<typename T>
void nameVkObject(vk::Device device, T object, const std::string& name) {
    uint64_t handle = *reinterpret_cast<uint64_t*>(&object);

    auto nameInfo = vk::DebugUtilsObjectNameInfoEXT()
        .setObjectHandle(handle)
        .setObjectType(T::objectType)
        .setPObjectName(name.c_str());

    ExtFunctions::setDebugUtilsObjectNameEXT(device, nameInfo);
}

void cmdBeginLabel(vk::CommandBuffer cmd, const std::string& name, std::array<float, 4> color);
void cmdEndLabel(vk::CommandBuffer cmd);
void cmdInsertLabel(vk::CommandBuffer cmd, const std::string& name, std::array<float, 4> color);

NAMESPACE_END(zvk::DebugUtils)