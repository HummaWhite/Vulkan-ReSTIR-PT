#pragma once

#include <vulkan/vulkan.hpp>

#include <util/NamespaceDecl.h>

NAMESPACE_BEGIN(zvk)

#if defined(ZVK_DEBUG)
const std::vector<const char*> ValidationLayers = {
	"VK_LAYER_KHRONOS_validation"
};
const bool EnableValidationLayer = true;
#else
const std::vector<const char*> ValidationLayers;
const bool EnableValidationLayer = false;
#endif

NAMESPACE_END(zvk)