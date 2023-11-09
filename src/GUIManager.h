#pragma once

#include <iostream>
#include <random>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "zvk.h"

class GUIManager : public zvk::BaseVkObject {
public:
    GUIManager(const zvk::Context* ctx, GLFWwindow* window, uint32_t numSwapchainImages);
    ~GUIManager() { destroy(); }

    void createDescriptorPool();
    void createRenderPass();
    void destroy();

    void beginFrame();

    void render(vk::CommandBuffer cmd, vk::Framebuffer framebuffer, vk::Extent2D extent);

private:
    std::unique_ptr<zvk::DescriptorPool> mDescriptorPool;
    vk::RenderPass mRenderPass;
};