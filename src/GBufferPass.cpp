#include "GBufferPass.h"
#include "Resource.h"

GBufferPass::GBufferPass(
	const zvk::Context* ctx, vk::Extent2D extent, const Resource& resource, vk::ImageLayout outLayout
) : zvk::BaseVkObject(ctx), mMultiDrawSupport(ctx->instance()->deviceFeatures.multiDrawIndirect)
{
	createDrawBuffer(resource);
	createResource(extent);
	createRenderPass(outLayout);
	createFramebuffer(extent);
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
		vk::ClearColorValue(0.f, 0.f, 0.f, 1.f),
		vk::ClearDepthStencilValue(1.f, 0)
	};

	auto renderPassBeginInfo = vk::RenderPassBeginInfo()
		.setRenderPass(mRenderPass)
		.setFramebuffer(framebuffer[inFlightIdx][curFrame])
		.setRenderArea({ { 0, 0 }, extent })
		.setClearValues(clearValues);

	cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
	constexpr auto bindPoint = vk::PipelineBindPoint::eGraphics;

	cmd.bindPipeline(bindPoint, mPipeline);

	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, CameraDescSet, param.cameraDescSet, {});
	cmd.bindDescriptorSets(bindPoint, mPipelineLayout, ResourceDescSet, param.resourceDescSet, {});

	cmd.bindVertexBuffers(0, param.vertexBuffer, vk::DeviceSize(0));
	cmd.bindIndexBuffer(param.indexBuffer, 0, vk::IndexType::eUint32);

	cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f));
	cmd.setScissor(0, vk::Rect2D({ 0, 0 }, extent));

	for (uint32_t i = 0; i < param.count; i++) {
		cmd.drawIndexedIndirect(
			mIndirectDrawBuffer->buffer, (param.offset + i) * sizeof(vk::DrawIndexedIndirectCommand), 1, sizeof(vk::DrawIndexedIndirectCommand)
		);
	}
	cmd.endRenderPass();
}

void GBufferPass::recreateFrame(vk::Extent2D extent) {
	destroyFrame();
	createResource(extent);
	createFramebuffer(extent);
}

void GBufferPass::createDrawBuffer(const Resource& resource) {
	std::vector<vk::DrawIndexedIndirectCommand> commands;

	for (const auto& model : resource.modelInstances[Resource::Object]) {
		glm::mat4 modelMatrix = model->modelMatrix();
		glm::mat4 modelInvT(glm::transpose(glm::inverse(modelMatrix)));
		uint32_t indexOffset = resource.meshInstances[Resource::Object][model->meshOffset()].indexOffset;
		commands.push_back({ model->numIndices(), 1, indexOffset, 0, numInstances++ });
	}

	mIndirectDrawBuffer = zvk::Memory::createBufferFromHost(
		mCtx, zvk::QueueIdx::GeneralUse, commands.data(), zvk::sizeOf(commands),
		vk::BufferUsageFlagBits::eIndirectBuffer
	);
	zvk::DebugUtils::nameVkObject(mCtx->device, mIndirectDrawBuffer->buffer, "GBufferIndirectDrawBuffer");
}

void GBufferPass::createResource(vk::Extent2D extent) {
	auto imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage |
		vk::ImageUsageFlagBits::eSampled;

	for (uint32_t i = 0; i < NumFramesInFlight; i++) {
		motionVector[i] = zvk::Memory::createImage2DAndInitLayout(
			mCtx, zvk::QueueIdx::GeneralUse, extent, MotionVectorFormat,
			vk::ImageTiling::eOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			imageUsage,
			vk::MemoryPropertyFlagBits::eDeviceLocal
		);
		motionVector[i]->createImageView();
		motionVector[i]->createSampler(vk::Filter::eLinear);

		zvk::DebugUtils::nameVkObject(mCtx->device, motionVector[i]->image, "motionVector[" + std::to_string(i) + "]");

		for (int j = 0; j < 2; j++) {

			depthNormal[i][j] = zvk::Memory::createImage2DAndInitLayout(
				mCtx, zvk::QueueIdx::GeneralUse, extent, DepthNormalFormat,
				vk::ImageTiling::eOptimal,
				vk::ImageLayout::eShaderReadOnlyOptimal,
				imageUsage,
				vk::MemoryPropertyFlagBits::eDeviceLocal
			);
			depthNormal[i][j]->createImageView();
			depthNormal[i][j]->createSampler(vk::Filter::eLinear);

			zvk::DebugUtils::nameVkObject(
				mCtx->device, depthNormal[i][j]->image, "depthNormal[" + std::to_string(i) + ", " + std::to_string(j) + "]"
			);

			albedoMatId[i][j] = zvk::Memory::createImage2DAndInitLayout(
				mCtx, zvk::QueueIdx::GeneralUse, extent, AlbedoMatIdFormat,
				vk::ImageTiling::eOptimal,
				vk::ImageLayout::eShaderReadOnlyOptimal,
				imageUsage,
				vk::MemoryPropertyFlagBits::eDeviceLocal
			);
			albedoMatId[i][j]->createImageView();
			albedoMatId[i][j]->createSampler(vk::Filter::eNearest);

			zvk::DebugUtils::nameVkObject(
				mCtx->device, albedoMatId[i][j]->image, "albedoMatId[" + std::to_string(i) + ", " + std::to_string(j) + "]"
			);

			mDepthStencil[i][j] = zvk::Memory::createImage2D(
				mCtx, extent, DepthStencilFormat,
				vk::ImageTiling::eOptimal,
				vk::ImageUsageFlagBits::eDepthStencilAttachment,
				vk::MemoryPropertyFlagBits::eDeviceLocal
			);
			mDepthStencil[i][j]->createImageView();
		}
	}
}

void GBufferPass::createRenderPass(vk::ImageLayout outLayout) {
	auto attachmentDesc = vk::AttachmentDescription()
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eUndefined);

	auto depthNormalDesc = attachmentDesc
		.setFormat(DepthNormalFormat)
		.setFinalLayout(outLayout);

	auto albedoMatIdxDesc = attachmentDesc
		.setFormat(AlbedoMatIdFormat)
		.setFinalLayout(outLayout);

	auto motionVectorDesc = attachmentDesc
		.setFormat(MotionVectorFormat)
		.setFinalLayout(outLayout);

	auto depthStencilDesc = attachmentDesc
		.setFormat(vk::Format::eD32Sfloat)
		.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

	auto attachments = { depthNormalDesc, albedoMatIdxDesc, motionVectorDesc, depthStencilDesc };

	vk::AttachmentReference depthNormalRef(0, vk::ImageLayout::eColorAttachmentOptimal);
	vk::AttachmentReference albedoMatIdRef(1, vk::ImageLayout::eColorAttachmentOptimal);
	vk::AttachmentReference motionVectorRef(2, vk::ImageLayout::eColorAttachmentOptimal);
	vk::AttachmentReference depthStencilRef(3, vk::ImageLayout::eDepthStencilAttachmentOptimal);

	auto colorRefs = { depthNormalRef, albedoMatIdRef, motionVectorRef };

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
			auto attachments = {
				depthNormal[i][j]->view, albedoMatId[i][j]->view, motionVector[i]->view, mDepthStencil[i][j]->view
			};

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
		shaderManager->createShaderModule("shaders/gbuffer.vert.spv"),
		vk::ShaderStageFlagBits::eVertex
	);

	auto fragStageInfo = zvk::ShaderManager::shaderStageCreateInfo(
		shaderManager->createShaderModule("shaders/gBuffer.frag.spv"),
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

	auto blendAttachments = { blendAttachment, blendAttachment, blendAttachment };

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
		throw std::runtime_error("Failed to create GBufferPass pipeline");
	}
	mPipeline = result.value;
}

void GBufferPass::destroyFrame() {
	for (uint32_t i = 0; i < NumFramesInFlight; i++) {
		mCtx->device.destroyFramebuffer(framebuffer[i][0]);
		mCtx->device.destroyFramebuffer(framebuffer[i][1]);
	}
}
