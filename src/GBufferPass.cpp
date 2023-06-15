#include "GBufferPass.h"
#include "shader/HostDevice.h"

GBufferPass::GBufferPass(
	const zvk::Context* ctx, vk::Extent2D extent, zvk::ShaderManager* shaderManager,
	std::vector<vk::DescriptorSetLayout>& descLayouts
) : zvk::BaseVkObject(ctx) {
	createResources(extent);
	createRenderPass();
	createFramebuffer(extent);
	createDescriptors();
	createPipeline(extent, shaderManager, descLayouts);
}

void GBufferPass::destroy() {
	delete mDescriptorSetLayout;
	delete mDescriptorPool;

	mCtx->device.destroyPipeline(mPipeline);
	mCtx->device.destroyPipelineLayout(mPipelineLayout);
	mCtx->device.destroyRenderPass(mRenderPass);

	destroyFrames();
}

void GBufferPass::render(
	vk::CommandBuffer cmd, vk::Buffer vertexBuffer,
	vk::Buffer indexBuffer, uint32_t indexOffset, uint32_t indexCount, vk::Extent2D extent
) {
	vk::ClearValue clearValues[] = {
			vk::ClearColorValue(0.f, 0.f, 0.f, 1.f),
			vk::ClearColorValue(0.f, 0.f, 0.f, 1.f),
			vk::ClearDepthStencilValue(1.f, 0.f)
	};

	auto renderPassBeginInfo = vk::RenderPassBeginInfo()
		.setRenderPass(mRenderPass)
		.setFramebuffer(framebuffer[0])
		.setRenderArea({ { 0, 0 }, extent })
		.setClearValues(clearValues);

	cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mPipeline);

	cmd.bindVertexBuffers(0, vertexBuffer, vk::DeviceSize(0));
	cmd.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint32);

	cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, extent.width, extent.height, 0.0f, 1.0f));
	cmd.setScissor(0, vk::Rect2D({ 0, 0 }, extent));

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, mPipelineLayout, 0, mDescriptorSet[0], {});

	//cmdBuffer.draw(VertexData.size(), 1, 0, 0);
	cmd.drawIndexed(indexCount, 1, indexOffset, 0, 0);

	cmd.endRenderPass();
}

void GBufferPass::updateDescriptors(const zvk::Buffer* uniforms, const zvk::Image* images) {
	vk::WriteDescriptorSet updates[] = {
		zvk::Descriptor::makeWrite(
			mDescriptorSetLayout, mDescriptorSet[0], 0,
			vk::DescriptorBufferInfo(uniforms->buffer, 0, uniforms->size)
		),
		zvk::Descriptor::makeWrite(
			mDescriptorSetLayout, mDescriptorSet[0], 1,
			vk::DescriptorImageInfo(images->sampler, images->imageView, images->layout)
		),
	};
	mCtx->device.updateDescriptorSets(updates, {});
}

void GBufferPass::swap() {
	std::swap(depthNormal[0], depthNormal[1]);
	std::swap(albedoMatIdx[0], albedoMatIdx[1]);
	std::swap(mDepthStencil[0], mDepthStencil[1]);
	std::swap(framebuffer[0], framebuffer[1]);
	std::swap(mDescriptorSet[0], mDescriptorSet[1]);
}

void GBufferPass::recreateFrames(vk::Extent2D extent) {
	destroyFrames();
	createResources(extent);
	createFramebuffer(extent);
}

void GBufferPass::createResources(vk::Extent2D extent) {
	for (int i = 0; i < 2; i++) {
		depthNormal[i] = zvk::Memory::createImage2D(
			mCtx, extent, DepthNormalFormat, vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		depthNormal[i]->createImageView();

		albedoMatIdx[i] = zvk::Memory::createImage2D(
			mCtx, extent, AlbedoMatIdxFormat, vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		albedoMatIdx[i]->createImageView();

		mDepthStencil[i] = zvk::Memory::createImage2D(
			mCtx, extent, DepthStencilFormat, vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		mDepthStencil[i]->createImageView();
	}
}

void GBufferPass::createRenderPass() {
	auto depthNormalDesc = vk::AttachmentDescription()
		.setFormat(DepthNormalFormat)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

	auto albedoMatIdxDesc = vk::AttachmentDescription()
		.setFormat(AlbedoMatIdxFormat)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

	auto depthStencilDesc = vk::AttachmentDescription()
		.setFormat(vk::Format::eD32Sfloat)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

	auto attachments = { depthNormalDesc, albedoMatIdxDesc, depthStencilDesc };

	vk::AttachmentReference depthNormalRef(0, vk::ImageLayout::eColorAttachmentOptimal);
	vk::AttachmentReference albedoMatIdxRef(1, vk::ImageLayout::eColorAttachmentOptimal);
	vk::AttachmentReference depthStencilRef(2, vk::ImageLayout::eDepthStencilAttachmentOptimal);

	auto colorRefs = { depthNormalRef, albedoMatIdxRef };

	auto subpass = vk::SubpassDescription()
		.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
		.setColorAttachments(colorRefs)
		.setPDepthStencilAttachment(&depthStencilRef);

	auto subpassDependency = vk::SubpassDependency()
		.setSrcSubpass(VK_SUBPASS_EXTERNAL)
		.setDstSubpass(0)
		.setSrcStageMask(
			vk::PipelineStageFlagBits::eColorAttachmentOutput |
			vk::PipelineStageFlagBits::eEarlyFragmentTests)
		.setDstStageMask(
			vk::PipelineStageFlagBits::eColorAttachmentOutput |
			vk::PipelineStageFlagBits::eEarlyFragmentTests)
		.setSrcAccessMask(vk::AccessFlagBits::eNone)
		.setDstAccessMask(
			vk::AccessFlagBits::eColorAttachmentWrite |
			vk::AccessFlagBits::eDepthStencilAttachmentWrite);

	auto createInfo = vk::RenderPassCreateInfo()
		.setAttachments(attachments)
		.setSubpasses(subpass)
		.setDependencies(subpassDependency);

	mRenderPass = mCtx->device.createRenderPass(createInfo);
}

void GBufferPass::createFramebuffer(vk::Extent2D extent) {
	for (int i = 0; i < 2; i++) {
		auto attachments = { depthNormal[i]->imageView, albedoMatIdx[i]->imageView, mDepthStencil[i]->imageView };

		auto createInfo = vk::FramebufferCreateInfo()
			.setRenderPass(mRenderPass)
			.setAttachments(attachments)
			.setWidth(extent.width)
			.setHeight(extent.height)
			.setLayers(1);

		framebuffer[i] = mCtx->device.createFramebuffer(createInfo);
	}
}

void GBufferPass::createPipeline(
	vk::Extent2D extent, zvk::ShaderManager* shaderManager,
	std::vector<vk::DescriptorSetLayout>& descLayouts
) {
	auto vertStageInfo = zvk::ShaderManager::shaderStageCreateInfo(
		shaderManager->createShaderModule("shaders/GBuffer.vert.spv"),
		vk::ShaderStageFlagBits::eVertex
	);

	auto fragStageInfo = zvk::ShaderManager::shaderStageCreateInfo(
		shaderManager->createShaderModule("shaders/GBuffer.frag.spv"),
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

	auto vertexBindingDesc = MeshVertex::bindingDescription();
	auto vertexAttribDesc = MeshVertex::attributeDescription();
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

	auto blendAttachments = { blendAttachment, blendAttachment };

	auto colorBlendStateInfo = vk::PipelineColorBlendStateCreateInfo()
		.setLogicOpEnable(false)
		.setLogicOp(vk::LogicOp::eCopy)
		.setAttachments(blendAttachments)
		.setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f });

	auto depthStencilStateInfo = vk::PipelineDepthStencilStateCreateInfo()
		.setDepthTestEnable(true)
		.setDepthWriteEnable(true)
		.setDepthCompareOp(vk::CompareOp::eLess)
		.setDepthBoundsTestEnable(false)
		.setStencilTestEnable(false);

	descLayouts.push_back(mDescriptorSetLayout->layout);

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
		throw std::runtime_error("Failed to create pipeline");
	}
	mPipeline = result.value;
}

void GBufferPass::createDescriptors() {
	std::vector<vk::DescriptorSetLayoutBinding> bindings = {
		zvk::Descriptor::makeBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex),
		zvk::Descriptor::makeBinding(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment),
	};
	mDescriptorSetLayout = new zvk::DescriptorSetLayout(mCtx, bindings);

	mDescriptorPool = new zvk::DescriptorPool(mCtx, { mDescriptorSetLayout }, 2);
	mDescriptorSet[0] = mDescriptorPool->allocDescriptorSet(mDescriptorSetLayout->layout);
	mDescriptorSet[1] = mDescriptorPool->allocDescriptorSet(mDescriptorSetLayout->layout);
}

void GBufferPass::destroyFrames() {
	for (int i = 0; i < 2; i++) {
		mCtx->device.destroyFramebuffer(framebuffer[i]);
		delete mDepthStencil[i];
		delete depthNormal[i];
		delete albedoMatIdx[i];
	}
}
