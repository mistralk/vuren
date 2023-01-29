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

        m_aoData.frameCount = 0;
        m_aoData.radius = 1.0;
    }

    void updateGui() {
        if (!ImGui::CollapsingHeader("Ambient Occlusion Pass"))
            return;

        ImGui::DragFloat("Radius (ray.maxT)", &m_aoData.radius, 0.01f, 0.0f, 100.0f, "%.02f");
    }

    void connectTextureWorldPos(const std::string &srcTexture) {
        m_pResourceManager->connectTextures(srcTexture, "AoInWorldPos");
    }

    void connectTextureWorldNormal(const std::string &srcTexture) {
        m_pResourceManager->connectTextures(srcTexture, "AoInWorldNormal");
    }

    void define() override {
        // prepare resources
        m_pResourceManager->createTextureRGBA32Sfloat("AoInWorldPos");
        m_pResourceManager->createTextureRGBA32Sfloat("AoInWorldNormal");
        m_pResourceManager->createTextureRGBA32Sfloat("AoOutput");
        auto outputTex = m_pResourceManager->getTexture("AoOutput");
        transitionImageLayout(*m_pContext, m_commandPool, outputTex, vk::ImageLayout::eUndefined,
                              vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits::eTopOfPipe,
                              vk::PipelineStageFlagBits::eRayTracingShaderKHR);

        // for gui output selection
        m_pContext->kOffscreenOutputTextureNames.push_back("AoOutput");

        // prepare the AO variable buffer
        m_pResourceManager->createUniformBuffer<AoData>("AoData");

        // create a descriptor set
        std::vector<ResourceBindingInfo> bindings = {
            { "Tlas", vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR, 1 },
            { "AoData", vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR, 1 },
            { "AoInWorldPos", vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR, 1 },
            { "AoInWorldNormal", vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR, 1 },
            { "AoOutput", vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR, 1 },
        };
        createDescriptorSet(bindings);

        setupRayTracingPipeline("shaders/RenderPasses/AmbientOcclusionPass/AO.rgen.spv",
                                "shaders/RenderPasses/AmbientOcclusionPass/AO.rmiss.spv",
                                "shaders/RenderPasses/AmbientOcclusionPass/AO.rchit.spv");
    }

    void record(vk::CommandBuffer commandBuffer) override {
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, m_pipeline);

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, m_pipelineLayout, 0, 1,
                                         &m_descriptorSet, 0, nullptr);

        commandBuffer.traceRaysKHR(&m_rgenRegion, &m_missRegion, &m_hitRegion, &m_callRegion, m_extent.width,
                                   m_extent.height, 1);
    }

    void updateUniformBuffer() {
        m_aoData.frameCount++;
        memcpy(m_pResourceManager->getMappedBuffer("AoData"), &m_aoData, sizeof(AoData));
    }

private:
    AoData m_aoData;
};

} // namespace vuren

#endif // AMBIENT_OCCLUSION_PASS_HPP