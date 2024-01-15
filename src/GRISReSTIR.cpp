#include "GRISReSTIR.h"
#include "shader/HostDevice.h"

#include <imgui.h>

void GRISReSTIR::destroy() {
}

void GRISReSTIR::render(vk::CommandBuffer cmd, vk::Extent2D extent, const zvk::DescriptorSetBindingMap& descSetBindings) {
    zvk::DebugUtils::cmdBeginLabel(cmd, "Path Tracing", { .5f, .2f, 1.f, 1.f }); {
        mPathTracePass->execute(cmd, extent, descSetBindings, &settings);
        zvk::DebugUtils::cmdEndLabel(cmd);
    }
    auto newResvMemoryBarrier = vk::MemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);
    /*
    cmd.pipelineBarrier(
        mPathTracePass->pipelineStage(), mRetracePass->pipelineStage(),
        vk::DependencyFlags{ 0 }, newResvMemoryBarrier, {}, {}
    );

    zvk::DebugUtils::cmdBeginLabel(cmd, "Path Retrace", { .6f, .4f, 1.f, 1.f }); {
        mRetracePass->execute(cmd, extent, descSetBindings, &settings);
        zvk::DebugUtils::cmdEndLabel(cmd);
    }
    auto rcDataMemoryBarrier = vk::MemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

    cmd.pipelineBarrier(
        mRetracePass->pipelineStage(), mTemporalReusePass->pipelineStage(),
        vk::DependencyFlags{ 0 }, rcDataMemoryBarrier, {}, {}
    );
    */

    cmd.pipelineBarrier(
        mPathTracePass->pipelineStage(), mTemporalReusePass->pipelineStage(),
        vk::DependencyFlags{ 0 }, newResvMemoryBarrier, {}, {}
    );

    zvk::DebugUtils::cmdBeginLabel(cmd, "Temporal Reuse", { .4f, .6f, 1.f, 1.f }); {
        mTemporalReusePass->execute(cmd, extent, descSetBindings, &settings);
        zvk::DebugUtils::cmdEndLabel(cmd);
    }
    auto temporalResvBarrier = vk::MemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

    cmd.pipelineBarrier(
        mTemporalReusePass->pipelineStage(), mSpatialReusePass->pipelineStage(),
        vk::DependencyFlags{ 0 }, temporalResvBarrier, {}, {}
    );

    zvk::DebugUtils::cmdBeginLabel(cmd, "Spatial Reuse", { .6f, .4f, 1.f, 1.f }); {
        mSpatialReusePass->execute(cmd, extent, descSetBindings, &settings);
        zvk::DebugUtils::cmdEndLabel(cmd);
    }
}

void GRISReSTIR::createPipeline(zvk::ShaderManager* shaderManager, const std::vector<vk::DescriptorSetLayout>& descLayouts) {
    mPathTracePass = std::make_unique<RayTracing>(mCtx);
    //mRetracePass = std::make_unique<RayTracing>(mCtx);
    mSpatialReusePass = std::make_unique<RayTracing>(mCtx);
    mTemporalReusePass = std::make_unique<RayTracing>(mCtx);
    //mMISWeightPass = std::make_unique<RayTracing>(mCtx);

    mPathTracePass->createPipeline(shaderManager, "shaders/gris_path_trace.comp.spv", "shaders/gris_path_trace.rgen.spv", descLayouts, sizeof(Settings));
    //mRetracePass->createPipeline(shaderManager, "shaders/gris_retrace.comp.spv", "", descLayouts, sizeof(Settings));
    mTemporalReusePass->createPipeline(shaderManager, "shaders/gris_resample_temporal.comp.spv", "", descLayouts, sizeof(Settings));
    mSpatialReusePass->createPipeline(shaderManager, "shaders/gris_resample_spatial.comp.spv", "", descLayouts, sizeof(Settings));
    //mMISWeightPass->createPipeline(shaderManager, "shaders/gris_mis_weight.comp.spv", "", descLayouts);
}

void GRISReSTIR::GUI(bool& resetFrame, bool& clearReservoir) {
    mPathTracePass->GUI();

    static const char* shiftMethods[] = { "Reconnection", "Replay", "Hybrid" };

    if (ImGui::Combo("Shift Mapping", reinterpret_cast<int*>(&settings.shiftType), shiftMethods, IM_ARRAYSIZE(shiftMethods))) {
        resetFrame = true;
        clearReservoir = true;
    }
    ImGui::SliderFloat("RR Scale", &settings.rrScale, 0.1f, 4.f);

    if (ImGui::Checkbox("Temporal Reuse", reinterpret_cast<bool*>(&settings.temporalReuse))) {
        resetFrame = true;
        clearReservoir = true;
    }
    ImGui::SameLine();

    if (ImGui::Checkbox("Spatial Reuse", reinterpret_cast<bool*>(&settings.spatialReuse))) {
        resetFrame = true;
        clearReservoir = true;
    }
}
