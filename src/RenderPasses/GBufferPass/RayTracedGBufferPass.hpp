#ifndef RAYTRACED_GBUFFER_PASS_HPP
#define RAYTRACED_GBUFFER_PASS_HPP

#include "GBufferCommon.h"
#include "RenderPass.hpp"

namespace vuren {

class RayTracedGBufferPass : public RayTracingRenderPass {
public:
    RayTracedGBufferPass() {}

    ~RayTracedGBufferPass() {}

    void init(VulkanContext *pContext, vk::CommandPool commandPool, std::shared_ptr<ResourceManager> pResourceManager,
              std::shared_ptr<Scene> pScene) override {
        RayTracingRenderPass::init(pContext, commandPool, pResourceManager, pScene);
    }

    void updateGui() {
    }

    void define() override {
        // for ray tracing, writing to output image will be manually called by shader
        m_pResourceManager->createTextureRGBA32Sfloat("RayTracedWorldPos");
        auto texture = m_pResourceManager->getTexture("RayTracedWorldPos");
        transitionImageLayout(*m_pContext, m_commandPool, texture, vk::ImageLayout::eUndefined,
                              vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits::eTopOfPipe,
                              vk::PipelineStageFlagBits::eRayTracingShaderKHR);

        m_pResourceManager->createTextureRGBA32Sfloat("RayTracedWorldNormal");
        texture = m_pResourceManager->getTexture("RayTracedWorldNormal");
        transitionImageLayout(*m_pContext, m_commandPool, texture, vk::ImageLayout::eUndefined,
                              vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits::eTopOfPipe,
                              vk::PipelineStageFlagBits::eRayTracingShaderKHR);

        m_pContext->kOffscreenOutputTextureNames.push_back("RayTracedWorldPos");
        m_pContext->kOffscreenOutputTextureNames.push_back("RayTracedWorldNormal");

        // create a descriptor set
        std::vector<ResourceBindingInfo> bindings = {
            { "CameraBuffer", vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR, 1 },
            { "SceneTextures", vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eClosestHitKHR,
              static_cast<uint32_t>(m_pScene->getTextures().size()) },
            { "SceneObjects", vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR,
              static_cast<uint32_t>(m_pScene->getObjects().size()) },
            { "RayTracedWorldPos", vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR, 1 },
            { "RayTracedWorldNormal", vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR, 1 },
            { "GBufferTlas", vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR,
              1 } // name doesn't matter for AS
        };
        createDescriptorSet(bindings);

        setupRayTracingPipeline("shaders/RenderPasses/GBufferPass/RayTracedGBuffer.rgen.spv",
                                "shaders/RenderPasses/GBufferPass/RayTracedGBuffer.rmiss.spv",
                                "shaders/RenderPasses/GBufferPass/RayTracedGBuffer.rchit.spv");
    }

    void record(vk::CommandBuffer commandBuffer) override {
        m_pcRay.clearColor     = vec4(0.0f, 1.0f, 0.0f, 0.0f);
        m_pcRay.lightIntensity = 1;

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, m_pipeline);

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, m_pipelineLayout, 0, 1,
                                         &m_descriptorSet, 0, nullptr);

        commandBuffer.pushConstants(m_pipelineLayout,
                                    vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR |
                                        vk::ShaderStageFlagBits::eMissKHR,
                                    0, sizeof(PushConstantRay), &m_pcRay);

        commandBuffer.traceRaysKHR(&m_rgenRegion, &m_missRegion, &m_hitRegion, &m_callRegion, m_extent.width,
                                   m_extent.height, 2);
    }

    void cleanup() { RayTracingRenderPass::cleanup(); }

private:
    PushConstantRay m_pcRay;
};

} // namespace vuren

#endif // RAYTRACED_GBUFFER_PASS_HPP