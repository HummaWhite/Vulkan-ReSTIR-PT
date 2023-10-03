#pragma once

#include <iostream>
#include <random>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/Context.h"
#include "core/Swapchain.h"
#include "core/Descriptor.h"

class GUIManager : public zvk::BaseVkObject {
public:
    GUIManager(const zvk::Context* ctx, const zvk::Swapchain* swapchain, GLFWwindow* window);
    ~GUIManager() { destroy(); }

    void createDescriptorPool();
    void createRenderPass();
    void destroy();

private:
    std::unique_ptr<zvk::DescriptorPool> mDescriptorPool;
    vk::RenderPass mRenderPass;
};