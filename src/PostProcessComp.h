#pragma once

#include "zvk.h"
#include "Scene.h"

class PostProcessComp : public zvk::BaseVkObject {
public:
	struct PushConstant {
		glm::ivec2 frameSize;
		uint32_t toneMapping;
	};

	PostProcessComp(const zvk::Context* ctx, const zvk::Swapchain* swapchain);
	~PostProcessComp() { destroy(); }
	void destroy();

	vk::DescriptorSetLayout descSetLayout() const { return mDescriptorSetLayout->layout; }

	void createPipeline(zvk::ShaderManager* shaderManager, const std::vector<vk::DescriptorSetLayout>& descLayouts);

	void render(vk::CommandBuffer cmd, vk::Extent2D extent, uint32_t imageIdx);
	void updateDescriptor(zvk::Image* inImage[2], const zvk::Swapchain* swapchain);
	void swap();

private:
	void createDescriptor(uint32_t nSwapchainImages);

public:
	uint32_t toneMapping = 0;

private:
	vk::Pipeline mPipeline;
	vk::PipelineLayout mPipelineLayout;

	zvk::DescriptorSetLayout* mDescriptorSetLayout = nullptr;
	zvk::DescriptorPool* mDescriptorPool = nullptr;
	std::vector<vk::DescriptorSet> mDescriptorSet[2];
};