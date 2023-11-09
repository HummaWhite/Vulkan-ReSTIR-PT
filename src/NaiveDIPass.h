#pragma once

#include "core/Context.h"
#include "core/ShaderManager.h"
#include "core/Memory.h"
#include "core/ShaderBindingTable.h"
#include "Scene.h"

struct RayTracingRenderParam;

class NaiveDIPass : public zvk::BaseVkObject {
public:
	NaiveDIPass(const zvk::Context* ctx) : BaseVkObject(ctx) {}
	~NaiveDIPass() { destroy(); }
	void destroy();

	void createPipeline(zvk::ShaderManager* shaderManager, uint32_t maxDepth, const std::vector<vk::DescriptorSetLayout>& descLayouts);

	void render(vk::CommandBuffer cmd, vk::Extent2D extent, const RayTracingRenderParam& param);

private:
	vk::Pipeline mRayTracingPipeline;
	vk::PipelineLayout mRayTracingPipelineLayout;
	std::unique_ptr<zvk::ShaderBindingTable> mShaderBindingTable;
};