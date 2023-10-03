#include "PostProcPassFrag.h"
#include "shader/HostDevice.h"

struct QuadVertex {
	constexpr static vk::VertexInputBindingDescription bindingDescription() {
		return vk::VertexInputBindingDescription(0, sizeof(QuadVertex), vk::VertexInputRate::eVertex);
	}

	constexpr static std::array<vk::VertexInputAttributeDescription, 2> attributeDescription() {
		return {
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(QuadVertex, pos)),
			vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, offsetof(QuadVertex, uv))
		};
	}
	std140(glm::vec2, pos);
	std140(glm::vec2, uv);
};

const QuadVertex QuadVertices[6] = {
	{ glm::vec2(0.f, 0.f), glm::vec2(0.f, 0.f) },
	{ glm::vec2(1.f, 0.f), glm::vec2(1.f, 0.f) },
	{ glm::vec2(1.f, 1.f), glm::vec2(1.f, 1.f) },
	{ glm::vec2(0.f, 0.f), glm::vec2(0.f, 0.f) },
	{ glm::vec2(1.f, 1.f), glm::vec2(1.f, 1.f) },
	{ glm::vec2(0.f, 1.f), glm::vec2(0.f, 1.f) },
};

PostProcPassFrag::PostProcPassFrag(const zvk::Context* ctx, const zvk::Swapchain* swapchain) : zvk::BaseVkObject(ctx) {
	createResource();
	createRenderPass();
	createFramebuffer(swapchain);
}

void PostProcPassFrag::destroy() {
	mCtx->device.destroyRenderPass(mRenderPass);
	mCtx->device.destroyPipeline(mPipeline);
	mCtx->device.destroyPipelineLayout(mPipelineLayout);
	destroyFrame();
}

void PostProcPassFrag::recreateFrame(const zvk::Swapchain* swapchain) {
	destroyFrame();
	createFramebuffer(swapchain);
}

void PostProcPassFrag::createPipeline(
	zvk::ShaderManager* shaderManager, vk::Extent2D extent,
	const std::vector<vk::DescriptorSetLayout>& descLayouts
) {
	auto vertStageInfo = zvk::ShaderManager::shaderStageCreateInfo(
		shaderManager->createShaderModule("shaders/postProc.vert.spv"),
		vk::ShaderStageFlagBits::eVertex
	);

	auto fragStageInfo = zvk::ShaderManager::shaderStageCreateInfo(
		shaderManager->createShaderModule("shaders/postProc.frag.spv"),
		vk::ShaderStageFlagBits::eFragment
	);

	auto shaderStages = { vertStageInfo, fragStageInfo };

	auto dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::Viewport viewport(0.0f, 0.0f, float(extent.width), float(extent.height), 0.0f, 1.0f);
	vk::Rect2D scissor({ 0, 0 }, extent);

	auto viewportStateInfo = vk::PipelineViewportStateCreateInfo()
		.setViewports(viewport)
		.setScissors(scissor);

	auto dynamicStateInfo = vk::PipelineDynamicStateCreateInfo()
		.setDynamicStates(dynamicStates);

	auto vertexBindingDesc = QuadVertex::bindingDescription();
	auto vertexAttribDesc = QuadVertex::attributeDescription();
	auto vertexInputStateInfo = vk::PipelineVertexInputStateCreateInfo()
		.setVertexBindingDescriptions(vertexBindingDesc)
		.setVertexAttributeDescriptions(vertexAttribDesc);

	auto inputAssemblyStateInfo = vk::PipelineInputAssemblyStateCreateInfo()
		.setTopology(vk::PrimitiveTopology::eTriangleList)
		.setPrimitiveRestartEnable(false);

	auto rasterizationStateInfo = vk::PipelineRasterizationStateCreateInfo()
		.setDepthClampEnable(false)
		.setRasterizerDiscardEnable(false)
		.setPolygonMode(vk::PolygonMode::eFill)
		.setLineWidth(1.0f)
		.setCullMode(vk::CullModeFlagBits::eBack)
		.setCullMode(vk::CullModeFlagBits::eNone)
		.setFrontFace(vk::FrontFace::eCounterClockwise)
		.setDepthBiasEnable(false);

	auto multisamplingStateInfo = vk::PipelineMultisampleStateCreateInfo()
		.setSampleShadingEnable(false)
		.setRasterizationSamples(vk::SampleCountFlagBits::e1);

	auto blendAttachment = vk::PipelineColorBlendAttachmentState()
		.setBlendEnable(false)
		.setSrcColorBlendFactor(vk::BlendFactor::eOne)
		.setDstColorBlendFactor(vk::BlendFactor::eZero)
		.setColorBlendOp(vk::BlendOp::eAdd)
		.setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
		.setDstAlphaBlendFactor(vk::BlendFactor::eZero)
		.setAlphaBlendOp(vk::BlendOp::eAdd)
		.setColorWriteMask(
			vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
			vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

	auto colorBlendStateInfo = vk::PipelineColorBlendStateCreateInfo()
		.setLogicOpEnable(false)
		.setLogicOp(vk::LogicOp::eCopy)
		.setAttachments(blendAttachment)
		.setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f });

	auto depthStencilStateInfo = vk::PipelineDepthStencilStateCreateInfo()
		.setDepthTestEnable(false)
		.setDepthWriteEnable(false)
		.setDepthCompareOp(vk::CompareOp::eNever)
		.setDepthBoundsTestEnable(false)
		.setStencilTestEnable(false);

	auto pipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo()
		.setSetLayouts(descLayouts);

	mPipelineLayout = mCtx->device.createPipelineLayout(pipelineLayoutCreateInfo);

	auto pipelineCreateInfo = vk::GraphicsPipelineCreateInfo()
		.setStages(shaderStages)
		.setPVertexInputState(&vertexInputStateInfo)
		.setPInputAssemblyState(&inputAssemblyStateInfo)
		.setPViewportState(&viewportStateInfo)
		.setPRasterizationState(&rasterizationStateInfo)
		.setPMultisampleState(&multisamplingStateInfo)
		.setPDepthStencilState(nullptr)
		.setPColorBlendState(&colorBlendStateInfo)
		.setPDynamicState(&dynamicStateInfo)
		.setPDepthStencilState(&depthStencilStateInfo)
		.setLayout(mPipelineLayout)
		.setRenderPass(mRenderPass)
		.setSubpass(0)
		.setBasePipelineHandle(nullptr)
		.setBasePipelineIndex(-1);

	auto result = mCtx->device.createGraphicsPipeline({}, pipelineCreateInfo);

	if (result.result != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to create PostProcPassFrag pipeline");
	}
	mPipeline = result.value;
}

void PostProcPassFrag::render(vk::CommandBuffer cmd, uint32_t frameIdx, uint32_t imageIdx, vk::DescriptorSet rayImageDescSet, vk::Extent2D extent) {
	vk::ClearValue clearValues[] = {
		vk::ClearColorValue(0.f, 0.f, 0.f, 1.f)
	};

	auto renderPassBeginInfo = vk::RenderPassBeginInfo()
		.setRenderPass(mRenderPass)
		.setFramebuffer(framebuffers[imageIdx])
		.setRenderArea({ { 0, 0 }, extent })
		.setClearValues(clearValues);

	cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mPipeline);
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, mPipelineLayout, RayImageDescSet, rayImageDescSet, {});

	cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, extent.width, extent.height, 0.0f, 1.0f));
	cmd.setScissor(0, vk::Rect2D({ 0, 0 }, extent));

	cmd.bindVertexBuffers(0, mQuadVertexBuffer->buffer, vk::DeviceSize(0));
	cmd.draw(6, 1, 0, 0);

	cmd.endRenderPass();
}

void PostProcPassFrag::createResource() {
	mQuadVertexBuffer = zvk::Memory::createBufferFromHost(
		mCtx, zvk::QueueIdx::GeneralUse, QuadVertices, 6 * sizeof(QuadVertex), vk::BufferUsageFlagBits::eVertexBuffer
	);
}

void PostProcPassFrag::createRenderPass() {
	auto colorDesc = vk::AttachmentDescription()
		.setFormat(SWAPCHAIN_FORMAT)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

	auto attachments = { colorDesc };

	vk::AttachmentReference colorRef(0, vk::ImageLayout::eColorAttachmentOptimal);

	auto colorRefs = { colorRef };

	auto subpass = vk::SubpassDescription()
		.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
		.setColorAttachments(colorRefs);

	auto subpassDependency = vk::SubpassDependency()
		.setSrcSubpass(VK_SUBPASS_EXTERNAL)
		.setDstSubpass(0)
		.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
		.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
		.setSrcAccessMask(vk::AccessFlagBits::eNone)
		.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

	auto createInfo = vk::RenderPassCreateInfo()
		.setAttachments(attachments)
		.setSubpasses(subpass)
		.setDependencies(subpassDependency);

	mRenderPass = mCtx->device.createRenderPass(createInfo);
}

void PostProcPassFrag::createFramebuffer(const zvk::Swapchain* swapchain) {
	framebuffers.resize(swapchain->numImages());

	for (int i = 0; i < swapchain->numImages(); i++) {
		auto createInfo = vk::FramebufferCreateInfo()
			.setRenderPass(mRenderPass)
			.setAttachments(swapchain->imageViews()[i])
			.setWidth(swapchain->extent().width)
			.setHeight(swapchain->extent().height)
			.setLayers(1);

		framebuffers[i] = mCtx->device.createFramebuffer(createInfo);
	}
}

void PostProcPassFrag::destroyFrame() {
	for (auto& frame : framebuffers) {
		mCtx->device.destroyFramebuffer(frame);
	}
}
