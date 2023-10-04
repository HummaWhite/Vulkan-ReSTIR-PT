#include "GBufferPass.h"
#include "shader/HostDevice.h"
#include "core/DebugUtils.h"

GBufferPass::GBufferPass(
	const zvk::Context* ctx, vk::Extent2D extent, const Resource& resource, vk::ImageLayout outLayout
) : zvk::BaseVkObject(ctx), mMultiDrawSupport(ctx->instance()->deviceFeatures.multiDrawIndirect)
{
	createDrawBuffer(resource);
	createResource(extent);
	createRenderPass(outLayout);
	createFramebuffer(extent);
	createDescriptor();
}

void GBufferPass::destroy() {
	mCtx->device.destroyPipeline(mPipeline);
	mCtx->device.destroyPipelineLayout(mPipelineLayout);
	mCtx->device.destroyRenderPass(mRenderPass);

	destroyFrame();
}

void GBufferPass::render(vk::CommandBuffer cmd, vk::Extent2D extent, uint32_t inFlightIdx, uint32_t curFrame, const GBufferRenderParam& param) {
	vk::ClearValue clearValues[] = {
		vk::ClearColorValue(0.f, 0.f, 0.f, 1.f),
		vk::ClearColorValue(0.f, 0.f, 0.f, 1.f),
		vk::ClearDepthStencilValue(1.f, 0)
	};

	auto renderPassBeginInfo = vk::RenderPassBeginInfo()
		.setRenderPass(mRenderPass)
		.setFramebuffer(framebuffer[inFlightIdx][curFrame])
		.setRenderArea({ { 0, 0 }, extent })
		.setClearValues(clearValues);

	cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
	auto bindPoint = vk::PipelineBindPoint::eGraphics;

	cmd.bindPipeline(bindPoint, mPipeline);

	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, CameraDescSet, param.cameraDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, ResourceDescSet, param.resourceDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, GBufferDrawParamDescSet, mDrawParamDescSet, {});

	cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, extent.width, extent.height, 0.0f, 1.0f));
	cmd.setScissor(0, vk::Rect2D({ 0, 0 }, extent));

	cmd.bindVertexBuffers(0, param.vertexBuffer, vk::DeviceSize(0));
	cmd.bindIndexBuffer(param.indexBuffer, 0, vk::IndexType::eUint32);

	for (uint32_t i = 0; i < param.count; i++) {
		cmd.pushConstants(
			mPipelineLayout, vk::ShaderStageFlagBits::eAllGraphics, 0, sizeof(GBufferDrawParam), &mDrawParams[param.offset + i]
		);

		cmd.drawIndexedIndirect(
			mDrawCommandBuffer->buffer, (param.offset + i) * sizeof(vk::DrawIndexedIndirectCommand), 1, sizeof(vk::DrawIndexedIndirectCommand)
		);
	}
	cmd.endRenderPass();
}

void GBufferPass::initDescriptor() {
	zvk::DescriptorWrite update;
	update.add(mDrawParamDescLayout.get(), mDrawParamDescSet, 0, zvk::Descriptor::makeBufferInfo(mDrawParamBuffer.get()));
	mCtx->device.updateDescriptorSets(update.writes, {});
}

void GBufferPass::recreateFrame(vk::Extent2D extent) {
	destroyFrame();
	createResource(extent);
	createFramebuffer(extent);
}

void GBufferPass::createDrawBuffer(const Resource& resource) {
	std::vector<vk::DrawIndexedIndirectCommand> commands;

	for (const auto& model : resource.modelInstances()) {
		glm::mat4 modelMatrix = model->modelMatrix();
		glm::mat4 modelInvT(glm::transpose(glm::inverse(modelMatrix)));

		for (uint32_t i = 0; i < model->numMeshes(); i++) {
			const auto& mesh = resource.meshInstances()[model->meshOffset() + i];
			commands.push_back({ mesh.indexCount, 1, mesh.indexOffset, 0, 0 });
			mDrawParams.push_back({ modelMatrix, modelInvT, mesh.materialIdx });
		}
	}

	mDrawCommandBuffer = zvk::Memory::createBufferFromHost(
		mCtx, zvk::QueueIdx::GeneralUse, commands.data(), zvk::sizeOf(commands),
		vk::BufferUsageFlagBits::eIndirectBuffer
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, mDrawCommandBuffer->buffer, "drawCommandBuffer");

	mDrawParamBuffer = zvk::Memory::createBufferFromHost(
		mCtx, zvk::QueueIdx::GeneralUse, mDrawParams.data(), zvk::sizeOf(mDrawParams),
		vk::BufferUsageFlagBits::eStorageBuffer
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, mDrawParamBuffer->buffer, "drawParamBuffer");
}

void GBufferPass::createResource(vk::Extent2D extent) {
	for (uint32_t i = 0; i < NumFramesInFlight; i++) {
		for (int j = 0; j < 2; j++) {
			GBufferA[i][j] = zvk::Memory::createImage2D(
				mCtx, extent, GBufferAFormat, vk::ImageTiling::eOptimal,
				vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
				vk::MemoryPropertyFlagBits::eDeviceLocal
			);
			GBufferA[i][j]->createImageView();
			GBufferA[i][j]->createSampler(vk::Filter::eNearest);

			zvk::DebugUtils::nameVkObject(
				mCtx->device, GBufferA[i][j]->image, "GBufferA[" + std::to_string(i) + ", " + std::to_string(j) + "]"
			);

			GBufferB[i][j] = zvk::Memory::createImage2D(
				mCtx, extent, GBufferBFormat, vk::ImageTiling::eOptimal,
				vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
				vk::MemoryPropertyFlagBits::eDeviceLocal
			);
			GBufferB[i][j]->createImageView();
			GBufferB[i][j]->createSampler(vk::Filter::eNearest);

			zvk::DebugUtils::nameVkObject(
				mCtx->device, GBufferB[i][j]->image, "GBufferB[" + std::to_string(i) + ", " + std::to_string(j) + "]"
			);

			mDepthStencil[i][j] = zvk::Memory::createImage2D(
				mCtx, extent, DepthStencilFormat, vk::ImageTiling::eOptimal,
				vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal
			);
			mDepthStencil[i][j]->createImageView();
		}
	}
}

void GBufferPass::createRenderPass(vk::ImageLayout outLayout) {
	auto depthNormalDesc = vk::AttachmentDescription()
		.setFormat(GBufferAFormat)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setFinalLayout(outLayout);

	auto albedoMatIdxDesc = vk::AttachmentDescription()
		.setFormat(GBufferBFormat)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setFinalLayout(outLayout);

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
	for (uint32_t i = 0; i < NumFramesInFlight; i++) {
		for (int j = 0; j < 2; j++) {
			auto attachments = { GBufferA[i][j]->view, GBufferB[i][j]->view, mDepthStencil[i][j]->view };

			auto createInfo = vk::FramebufferCreateInfo()
				.setRenderPass(mRenderPass)
				.setAttachments(attachments)
				.setWidth(extent.width)
				.setHeight(extent.height)
				.setLayers(1);

			framebuffer[i][j] = mCtx->device.createFramebuffer(createInfo);
		}
	}
}

void GBufferPass::createPipeline(
	vk::Extent2D extent, zvk::ShaderManager* shaderManager,
	const std::vector<vk::DescriptorSetLayout>& descLayouts
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

	vk::PushConstantRange pushConstant(vk::ShaderStageFlagBits::eAllGraphics, 0, sizeof(GBufferDrawParam));

	auto pipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo()
		.setSetLayouts(descLayouts)
		.setPushConstantRanges(pushConstant);

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
		throw std::runtime_error("Failed to create GBufferPass pipeline");
	}
	mPipeline = result.value;
}

void GBufferPass::createDescriptor() {
	std::vector<vk::DescriptorSetLayoutBinding> drawBindings = {
		zvk::Descriptor::makeBinding(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex)
	};

	mDrawParamDescLayout = std::make_unique<zvk::DescriptorSetLayout>(mCtx, drawBindings);
	mDescriptorPool = std::make_unique<zvk::DescriptorPool>(mCtx, mDrawParamDescLayout.get(), 1);
	mDrawParamDescSet = mDescriptorPool->allocDescriptorSet(mDrawParamDescLayout->layout);
}

void GBufferPass::destroyFrame() {
	for (uint32_t i = 0; i < NumFramesInFlight; i++) {
		mCtx->device.destroyFramebuffer(framebuffer[i][0]);
		mCtx->device.destroyFramebuffer(framebuffer[i][1]);
	}
}
