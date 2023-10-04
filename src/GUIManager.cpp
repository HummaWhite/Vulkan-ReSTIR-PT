#include "GUIManager.h"
#include "core/Command.h"
#include "shader/HostDevice.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

GUIManager::GUIManager(const zvk::Context* ctx, GLFWwindow* window, uint32_t numSwapchainImages) :
    BaseVkObject(ctx)
{
	createDescriptorPool();
	createRenderPass();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	ImGui::StyleColorsDark();
	auto& guiStyle = ImGui::GetStyle();
	guiStyle.FrameRounding = 1.0f;
	guiStyle.FramePadding.y = 2.0f;
	guiStyle.ItemSpacing.y = 6.0f;
	guiStyle.GrabRounding = 1.0f;

	ImGui_ImplGlfw_InitForVulkan(window, true);

	auto queueIdx = zvk::QueueIdx::GeneralUse;

	auto initInfo = ImGui_ImplVulkan_InitInfo {
		.Instance = ctx->instance()->instance(),
		.PhysicalDevice = ctx->instance()->physicalDevice(),
		.Device = ctx->device,
		.QueueFamily = ctx->queues[queueIdx].familyIdx,
		.Queue = ctx->queues[queueIdx].queue,
		.PipelineCache = VK_NULL_HANDLE,
		.DescriptorPool = mDescriptorPool->pool,
		.Subpass = 0,
		.MinImageCount = 2,
		.ImageCount = numSwapchainImages,
		.MSAASamples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
		.Allocator = nullptr,
		.CheckVkResultFn = nullptr,
	};

	ImGui_ImplVulkan_Init(&initInfo, mRenderPass);

	auto cmd = zvk::Command::createOneTimeSubmit(ctx, queueIdx); {
		ImGui_ImplVulkan_CreateFontsTexture(cmd->cmd);
	}
	cmd->submitAndWait();

	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void GUIManager::createDescriptorPool() {
	auto sizes = {
		vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1),
	};
	mDescriptorPool = std::make_unique<zvk::DescriptorPool>(mCtx, sizes);
}

void GUIManager::createRenderPass() {
	auto colorDesc = vk::AttachmentDescription()
		.setFormat(SWAPCHAIN_FORMAT)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eLoad)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
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

void GUIManager::destroy() {
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	mCtx->device.destroyRenderPass(mRenderPass);
}

void GUIManager::beginFrame() {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void GUIManager::render(vk::CommandBuffer cmd, vk::Framebuffer framebuffer, vk::Extent2D extent) {
	ImGui::Render();

	vk::ClearValue clearValues[] = {
		vk::ClearColorValue(0.f, 0.f, 0.f, 1.f)
	};

	auto renderPassBeginInfo = vk::RenderPassBeginInfo()
		.setRenderPass(mRenderPass)
		.setFramebuffer(framebuffer)
		.setRenderArea({ { 0, 0 }, extent })
		.setClearValues(clearValues);

	cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	cmd.endRenderPass();
}
