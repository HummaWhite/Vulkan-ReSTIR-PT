#pragma once

#include <vulkan/vulkan.hpp>

#include <util/NamespaceDecl.h>

#define ZVK_VALIDATION_LAYER true

NAMESPACE_BEGIN(zvk)

#if ZVK_VALIDATION_LAYER
const std::vector<const char*> ValidationLayers = {
	"VK_LAYER_KHRONOS_validation"
};
#endif

NAMESPACE_END(zvk)