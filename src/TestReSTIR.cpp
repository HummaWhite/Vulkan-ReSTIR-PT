#include "TestReSTIR.h"
#include "shader/HostDevice.h"

#include <imgui.h>

void TestReSTIR::destroy() {
}

void TestReSTIR::render(vk::CommandBuffer cmd, vk::Extent2D extent, const zvk::DescriptorSetBindingMap& descSetBindings) {
    return;
    zvk::DebugUtils::cmdBeginLabel(cmd, "Path Gen", { .5f, .2f, 1.f, 1.f }); {
        mPathTracePass->execute(cmd, extent, descSetBindings, &settings);
        zvk::DebugUtils::cmdEndLabel(cmd);
    }
    auto newResvMemoryBarrier = vk::MemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

    cmd.pipelineBarrier(
        mPathTracePass->pipelineStage(), mTemporalReusePass->pipelineStage(),
        vk::DependencyFlags{ 0 }, newResvMemoryBarrier, {}, {}
    );

    zvk::DebugUtils::cmdBeginLabel(cmd, "Temporal Reuse", { .6f, .4f, 1.f, 1.f }); {
        mTemporalReusePass->execute(cmd, extent, descSetBindings, &settings);
        zvk::DebugUtils::cmdEndLabel(cmd);
    }
    auto rcDataMemoryBarrier = vk::MemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

    cmd.pipelineBarrier(
        mTemporalReusePass->pipelineStage(), mSpatialReusePass->pipelineStage(),
        vk::DependencyFlags{ 0 }, rcDataMemoryBarrier, {}, {}
    );

    zvk::DebugUtils::cmdBeginLabel(cmd, "Spatial Reuse", { .4f, .6f, 1.f, 1.f }); {
        mSpatialReusePass->execute(cmd, extent, descSetBindings, &settings);
        zvk::DebugUtils::cmdEndLabel(cmd);
    }
}

void TestReSTIR::createPipeline(zvk::ShaderManager* shaderManager, const std::vector<vk::DescriptorSetLayout>& descLayouts) {
    return;
    mPathTracePass = std::make_unique<RayTracing>(mCtx);
    mSpatialReusePass = std::make_unique<RayTracing>(mCtx);
    mTemporalReusePass = std::make_unique<RayTracing>(mCtx);

    mPathTracePass->createPipeline(shaderManager, "shaders/di_path_gen.comp.spv", "shaders/di_path_gen.rgen.spv", descLayouts, sizeof(Settings));
    mTemporalReusePass->createPipeline(shaderManager, "shaders/di_temporal.comp.spv", "shaders/di_temporal.rgen.spv", descLayouts, sizeof(Settings));
    mSpatialReusePass->createPipeline(shaderManager, "shaders/di_spatial.comp.spv", "shaders/di_spatial.rgen.spv", descLayouts, sizeof(Settings));
}

void TestReSTIR::GUI(bool& resetFrame, bool& clearReservoir) {
    mPathTracePass->GUI();

    static const char* shiftMethods[] = { "Reconnection", "Replay", "Hybrid" };

    if (ImGui::Combo("Shift Mapping", reinterpret_cast<int*>(&settings.shiftType), shiftMethods, IM_ARRAYSIZE(shiftMethods))) {
        resetFrame = true;
        clearReservoir = true;
    }

    static const char* sampleTypes[] = { "Light", "BSDF", "Both" };

    if (ImGui::Combo("Sample Type", reinterpret_cast<int*>(&settings.sampleType), sampleTypes, IM_ARRAYSIZE(sampleTypes))) {
        resetFrame = true;
        clearReservoir = true;
    }

    if (ImGui::Checkbox("TemporalReuse", reinterpret_cast<bool*>(&settings.temporalReuse))) {
        resetFrame = true;
        clearReservoir = true;
    }
    ImGui::SameLine();

    if (ImGui::Checkbox("SpatialReuse", reinterpret_cast<bool*>(&settings.spatialReuse))) {
        resetFrame = true;
        clearReservoir = true;
    }
}
