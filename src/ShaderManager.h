#pragma once

#include <vulkan/vulkan.hpp>

#include <iostream>
#include <fstream>
#include <map>

#include "util/File.h"
#include "util/NamespaceDecl.h"

NAMESPACE_BEGIN(zvk)

enum class ShaderLoadOp {
	Normal, NoRecord, InstantDestroy
};

class ShaderManager {
public:
	ShaderManager() = default;
	ShaderManager(vk::Device device) : mDevice(device) {}

	void destroyShaderModules();

	vk::ShaderModule createShaderModule(
		const File::path& path,
		ShaderLoadOp operation = ShaderLoadOp::Normal);

	static vk::PipelineShaderStageCreateInfo shaderStageCreateInfo(
		vk::ShaderModule module,
		vk::ShaderStageFlagBits stage,
		const char* entrance = "main");

private:
	vk::Device mDevice;
	std::map<File::path, vk::ShaderModule> mLoadedShaders;
};

NAMESPACE_END(zvk)