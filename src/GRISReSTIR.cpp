#include "GRISReSTIR.h"
#include "shader/HostDevice.h"

#include <imgui.h>

void GRISReSTIR::destroy() {
}

void GRISReSTIR::render(vk::CommandBuffer cmd, vk::Extent2D extent, const zvk::DescriptorSetBindingMap& descSetBindings) {
    mPathTracePass->execute(cmd, extent, descSetBindings, &settings);
}

void GRISReSTIR::createPipeline(zvk::ShaderManager* shaderManager, const std::vector<vk::DescriptorSetLayout>& descLayouts) {
    mPathTracePass = std::make_unique<RayTracing>(mCtx);
    mSpatialRetracePass = std::make_unique<zvk::ComputePipeline>(mCtx);
    mTemporalRetracePass = std::make_unique<zvk::ComputePipeline>(mCtx);
    mSpatialReusePass = std::make_unique<zvk::ComputePipeline>(mCtx);
    mTemporalReusePass = std::make_unique<zvk::ComputePipeline>(mCtx);
    mMISWeightPass = std::make_unique<zvk::ComputePipeline>(mCtx);

    mPathTracePass->createPipeline(shaderManager, "shaders/gris_path_trace.comp.spv", "shaders/gris_path_trace.rgen.spv", descLayouts, sizeof(Settings));
    mSpatialRetracePass->createPipeline(shaderManager, "shaders/gris_retrace_spatial.comp.spv", descLayouts);
    mTemporalRetracePass->createPipeline(shaderManager, "shaders/gris_retrace_temporal.comp.spv", descLayouts);
    mSpatialReusePass->createPipeline(shaderManager, "shaders/gris_resample_spatial.comp.spv", descLayouts);
    mTemporalReusePass->createPipeline(shaderManager, "shaders/gris_resample_temporal.comp.spv", descLayouts);
    mMISWeightPass->createPipeline(shaderManager, "shaders/gris_mis_weight.comp.spv", descLayouts);
}

void GRISReSTIR::GUI(bool& resetFrame, bool& clearReservoir) {
    mPathTracePass->GUI();

    static const char* shiftMethods[] = { "Reconnection", "Replay", "Hybrid" };

    if (ImGui::Combo("Shift Mapping", reinterpret_cast<int*>(&settings.shiftType), shiftMethods, IM_ARRAYSIZE(shiftMethods))) {
        resetFrame = true;
        clearReservoir = true;
    }
    ImGui::SliderFloat("RR Scale", &settings.rrScale, 0.1f, 4.f);
}
