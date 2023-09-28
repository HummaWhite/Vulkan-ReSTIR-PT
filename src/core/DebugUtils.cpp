#include "DebugUtils.h"
#include "core/ExtFunctions.h"

NAMESPACE_BEGIN(zvk::DebugUtils)

void cmdBeginLabel(vk::CommandBuffer cmd, const std::string& name, std::array<float, 4> color) {
    auto label = vk::DebugUtilsLabelEXT()
        .setPLabelName(name.c_str())
        .setColor(color);

    ExtFunctions::cmdBeginDebugUtilsLabelEXT(cmd, label);
}

void cmdEndLabel(vk::CommandBuffer cmd) {
    ExtFunctions::cmdEndDebugUtilsLabelEXT(cmd);
}

void cmdInsertLabel(vk::CommandBuffer cmd, const std::string& name, std::array<float, 4> color) {
    auto label = vk::DebugUtilsLabelEXT()
        .setPLabelName(name.c_str())
        .setColor(color);
    ExtFunctions::cmdInsertDebugUtilsLabelEXT(cmd, label);
}

NAMESPACE_END(zvk::DebugUtils)