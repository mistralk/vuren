#ifndef AMBIENT_OCCLUSION_PASS_HPP
#define AMBIENT_OCCLUSION_PASS_HPP

#include "AoCommon.h"
#include "RenderPass.hpp"

namespace vuren {
class AmbientOcclusionPass : public RayTracingRenderPass {
public:
    AmbientOcclusionPass() {}

    ~AmbientOcclusionPass() {}

    void init(VulkanContext *pContext, vk::CommandPool commandPool, std::shared_ptr<ResourceManager> pResourceManager,
              std::shared_ptr<Scene> pScene) override {
        RayTracingRenderPass::init(pContext, commandPool, pResourceManager, pScene);
    }

    void define() override {
        m_pResourceManager->createTextureRGBA32Sfloat("AOOutput");
        auto texture = m_pResourceManager->getTexture("AOOutput");
        transitionImageLayout(*m_pContext, m_commandPool, texture, vk::ImageLayout::eUndefined,
                              vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits::eTopOfPipe,
                              vk::PipelineStageFlagBits::eRayTracingShaderKHR);

        m_pContext->kOffscreenOutputTextureNames.push_back("AOOutput");

        // prepare the AO variable buffer
        m_pResourceManager->createUniformBuffer<AoData>("AoData");

        // create a descriptor set
        std::vector<ResourceBindingInfo> bindings = {
            { "Tlas", vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR, 1 },
            { "AoData", vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR, 1 },
            { "RasterPosWorld", vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR, 1 },
            { "RasterNormalWorld", vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR, 1 },
            { "AOOutput", vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR, 1 },
        };
        createDescriptorSet(bindings);

        setupRayTracingPipeline("shaders/RenderPasses/AmbientOcclusionPass/AO.rgen.spv",
                                "shaders/RenderPasses/AmbientOcclusionPass/AO.rmiss.spv",
                                "shaders/RenderPasses/AmbientOcclusionPass/AO.rchit.spv");
    }

    void record(vk::CommandBuffer commandBuffer) override {
        auto texture = m_pResourceManager->getTexture("RasterPosWorld");
        transitionImageLayout(commandBuffer, texture, vk::ImageLayout::eColorAttachmentOptimal,
                              vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits::eAllGraphics,
                              vk::PipelineStageFlagBits::eRayTracingShaderKHR);

        texture = m_pResourceManager->getTexture("RasterNormalWorld");
        transitionImageLayout(commandBuffer, texture, vk::ImageLayout::eColorAttachmentOptimal,
                              vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits::eAllGraphics,
                              vk::PipelineStageFlagBits::eRayTracingShaderKHR);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, m_pipeline);

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, m_pipelineLayout, 0, 1,
                                         &m_descriptorSet, 0, nullptr);

        commandBuffer.traceRaysKHR(&m_rgenRegion, &m_missRegion, &m_hitRegion, &m_callRegion, m_extent.width,
                                   m_extent.height, 1);
    }

    void updateAoDataUniformBuffer(float radius, unsigned int frameCount) {
        m_aoData.radius     = radius;
        m_aoData.frameCount = frameCount;
        memcpy(m_pResourceManager->getMappedBuffer("AoData"), &m_aoData, sizeof(AoData));
    }

private:
    AoData m_aoData;
};

} // namespace vuren

#endif // AMBIENT_OCCLUSION_PASS_HPP