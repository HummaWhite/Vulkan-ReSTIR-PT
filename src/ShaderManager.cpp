#include "ShaderManager.h"

#include "util/Error.h"

void ShaderManager::destroyShaderModules()
{
	for (auto& pair : mLoadedShaders)
		mDevice.destroyShaderModule(pair.second);
	mLoadedShaders.clear();
}

vk::ShaderModule ShaderManager::createShaderModule(const File::path& path, ShaderLoadOp operation)
{
	if (mLoadedShaders.find(path) != mLoadedShaders.end())
		return mLoadedShaders[path];

	Log::bracketLine<0>("Loading shader: " + File::absolute(path).generic_string());

	std::ifstream file(File::absolute(path), std::ios::ate | std::ios::binary);
	if (!file.is_open())
		Log::exit("not found");
	size_t size = file.tellg();

	std::vector<char> buffer(size);
	file.seekg(0);
	file.read(buffer.data(), size);

	auto createInfo = vk::ShaderModuleCreateInfo()
		.setPCode(reinterpret_cast<uint32_t*>(buffer.data()))
		.setCodeSize(buffer.size());

	auto shaderModule = mDevice.createShaderModule(createInfo);
	if (operation == ShaderLoadOp::Normal)
		mLoadedShaders[path] = shaderModule;
	return shaderModule;
}

vk::PipelineShaderStageCreateInfo ShaderManager::shaderStageCreateInfo(
	vk::ShaderModule module,
	vk::ShaderStageFlagBits stage,
	const char* entrance)
{
	return vk::PipelineShaderStageCreateInfo()
		.setModule(module)
		.setStage(stage)
		.setPName(entrance);
}
