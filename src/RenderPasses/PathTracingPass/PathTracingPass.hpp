#ifndef PATH_TRACING_PASS_HPP
#define PATH_TRACING_PASS_HPP

#include "PtCommon.h"
#include "RenderPass.hpp"

namespace vuren {
class PathTracingPass : public RayTracingRenderPass {
public:
    PathTracingPass() {}

    ~PathTracingPass() {}

    void init(VulkanContext *pContext, vk::CommandPool commandPool, std::shared_ptr<ResourceManager> pResourceManager,
              std::shared_ptr<Scene> pScene) override {
        RayTracingRenderPass::init(pContext, commandPool, pResourceManager, pScene);

        m_frameData.frameCount = 0;
    }

    void updateGui() {
        if (!ImGui::CollapsingHeader("Path Tracing Pass"))
            return;
    }

    void connectTextureWorldPos(const std::string &srcTexture) {
        m_pResourceManager->connectTextures(srcTexture, "PtInWorldPos");
    }

    void connectTextureWorldNormal(const std::string &srcTexture) {
        m_pResourceManager->connectTextures(srcTexture, "PtInWorldNormal");
    }

    void define() override {
        // prepare resources
        m_pResourceManager->createTextureRGBA32Sfloat("PtInWorldPos");
        m_pResourceManager->createTextureRGBA32Sfloat("PtInWorldNormal");
        m_pResourceManager->createTextureRGBA32Sfloat("PtOutput");
        auto outputTex = m_pResourceManager->getTexture("PtOutput");
        transitionImageLayout(*m_pContext, m_commandPool, outputTex, vk::ImageLayout::eUndefined,
                              vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits::eTopOfPipe,
                              vk::PipelineStageFlagBits::eRayTracingShaderKHR);

        // for gui output selection
        m_pContext->kOffscreenOutputTextureNames.push_back("PtOutput");

        // prepare the Pt variable buffer
        m_pResourceManager->createUniformBuffer<FrameData>("FrameData");

        // create a descriptor set
        std::vector<ResourceBindingInfo> bindings = {
            { "Tlas", vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR, 1 },
            { "FrameData", vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR, 1 },
            { "PtInWorldPos", vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR, 1 },
            { "PtInWorldNormal", vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR, 1 },
            { "PtOutput", vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR, 1 },
            { "SceneTextures", vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eClosestHitKHR,
              static_cast<uint32_t>(m_pScene->getTextures().size()) },
            { "SceneObjects", vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR,
              static_cast<uint32_t>(m_pScene->getObjects().size()) },
            { "SceneMaterials", vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR,
              static_cast<uint32_t>(m_pScene->getMaterials().size()) }
        };
        createDescriptorSet(bindings);

        setupRayTracingPipeline("shaders/RenderPasses/PathTracingPass/pt.rgen.spv",
                                "shaders/RenderPasses/PathTracingPass/pt.rmiss.spv",
                                "shaders/RenderPasses/PathTracingPass/pt.rchit.spv");
    }

    void record(vk::CommandBuffer commandBuffer) override {
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, m_pipeline);

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, m_pipelineLayout, 0, 1,
                                         &m_descriptorSet, 0, nullptr);

        commandBuffer.traceRaysKHR(&m_rgenRegion, &m_missRegion, &m_hitRegion, &m_callRegion, m_extent.width,
                                   m_extent.height, 1);
    }

    void updateUniformBuffer() {
        m_frameData.frameCount++;
        memcpy(m_pResourceManager->getMappedBuffer("FrameData"), &m_frameData, sizeof(FrameData));
    }

private:
    FrameData m_frameData;
};

} // namespace vuren

#endif // PATH_TRACING_PASS_HPP