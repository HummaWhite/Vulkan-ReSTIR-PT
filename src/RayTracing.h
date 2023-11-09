#pragma once

#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <iostream>
#include <optional>
#include <memory>
#include <limits>
#include <set>

#include "Scene.h"

#include "util/Error.h"
#include "util/Timer.h"

struct RayTracingRenderParam {
	vk::DescriptorSet cameraDescSet;
	vk::DescriptorSet resourceDescSet;
	vk::DescriptorSet rayImageDescSet;
	vk::DescriptorSet rayTracingDescSet;
	uint32_t maxDepth;
};