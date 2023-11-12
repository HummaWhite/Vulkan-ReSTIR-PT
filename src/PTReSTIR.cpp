#include "PTReSTIR.h"
#include "RayTracing.h"
#include "shader/HostDevice.h"

void PTReSTIR::destroy() {
}

void PTReSTIR::render(vk::CommandBuffer cmd, vk::Extent2D extent, const RayTracingRenderParam& param) {
}

void PTReSTIR::createPipeline(zvk::ShaderManager* shaderManager, const std::vector<vk::DescriptorSetLayout>& descLayouts) {
    mPathTracePass = std::make_unique<ComputeRayTrace>(mCtx);
    mSpatialRetracePass = std::make_unique<ComputeRayTrace>(mCtx);
    mTemporalRetracePass = std::make_unique<ComputeRayTrace>(mCtx);
    mSpatialReusePass = std::make_unique<ComputeRayTrace>(mCtx);
    mTemporalReusePass = std::make_unique<ComputeRayTrace>(mCtx);
    mMISWeightPass = std::make_unique<ComputeRayTrace>(mCtx);

    mPathTracePass->createPipeline(shaderManager, "shaders/gris_path_trace.comp.spv", descLayouts);
    mSpatialRetracePass->createPipeline(shaderManager, "shaders/gris_retrace_spatial.comp.spv", descLayouts);
    mTemporalRetracePass->createPipeline(shaderManager, "shaders/gris_retrace_temporal.comp.spv", descLayouts);
    mSpatialReusePass->createPipeline(shaderManager, "shaders/gris_resample_spatial.comp.spv", descLayouts);
    mTemporalReusePass->createPipeline(shaderManager, "shaders/gris_resample_temporal.comp.spv", descLayouts);
    mMISWeightPass->createPipeline(shaderManager, "shaders/gris_mis_weight.comp.spv", descLayouts);
}
