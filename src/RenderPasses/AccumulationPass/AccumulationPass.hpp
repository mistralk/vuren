#ifndef ACCUMULATION_PASS_HPP
#define ACCUMULATION_PASS_HPP

#include "RenderPass.hpp"
#include "AccumCommon.h"

namespace vuren {
class AccumulationPass : public RasterRenderPass {
public:
    AccumulationPass() {}

    ~AccumulationPass() {}

    void init(VulkanContext *pContext, vk::CommandPool commandPool, std::shared_ptr<ResourceManager> pResourceManager,
              std::shared_ptr<Scene> pScene) override {
        RasterRenderPass::init(pContext, commandPool, pResourceManager, pScene);

        m_accumData.frameCount = 0;
        m_lastCameraView = m_pScene->getCamera().getData().view;
    }

    void updateGui() {
        if (!ImGui::CollapsingHeader("Accumulation Pass"))
            return;

        ImGui::Text(" %d frames accumulated", m_accumData.frameCount);
    }

    void updateUniformBuffer() {
        // camera has moved
        if (m_lastCameraView != m_pScene->getCamera().getData().view) {
            m_lastCameraView = m_pScene->getCamera().getData().view;
            m_accumData.frameCount = 0;
        }

        else
            m_accumData.frameCount++;
        
        memcpy(m_pResourceManager->getMappedBuffer("AccumData"), &m_accumData, sizeof(AccumData));
    }

    void connectTextureCurrentFrame(const std::string &srcTexture) {
        m_pResourceManager->connectTextures(srcTexture, "CurrentFrame");
    }

    void define() override {
        m_pResourceManager->createTextureRGBA32Sfloat("CurrentFrame");
        m_pResourceManager->createTextureRGBA32Sfloat("PreviousFrames");
        m_pResourceManager->createTextureRGBA32Sfloat("AccumulatedOutput");
        m_pResourceManager->createDepthTexture("AccumDepth");
        m_pResourceManager->createUniformBuffer<AccumData>("AccumData");

        m_pContext->kOffscreenOutputTextureNames.push_back("AccumulatedOutput");
        
        transitionImageLayout(*m_pContext, m_commandPool, m_pResourceManager->getTexture("PreviousFrames"), vk::ImageLayout::eUndefined,
                              vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits::eAllCommands,
                              vk::PipelineStageFlagBits::eFragmentShader);

        // create a descriptor set
        std::vector<ResourceBindingInfo> bindings;
        bindings.push_back(
            { "CurrentFrame", vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1 });
        bindings.push_back(
            { "PreviousFrames", vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eFragment, 1 });
        bindings.push_back({ "AccumData", vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment, 1 });

        createDescriptorSet(bindings);

        // create framebuffers for the attachments
        std::vector<AttachmentInfo> colorAttachments = {
            { .imageView     = m_pResourceManager->getTexture("AccumulatedOutput")->descriptorInfo.imageView,
              .format        = vk::Format::eR32G32B32A32Sfloat,
              .oldLayout     = vk::ImageLayout::eUndefined,
              .newLayout     = vk::ImageLayout::eColorAttachmentOptimal,
              .srcStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput,
              .dstStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput,
              .srcAccessMask = vk::AccessFlagBits::eNone,
              .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite }
        };

        AttachmentInfo depthStencilAttachment = {
            .imageView     = m_pResourceManager->getTexture("AccumDepth")->descriptorInfo.imageView,
            .format        = findDepthFormat(*m_pContext),
            .oldLayout     = vk::ImageLayout::eUndefined,
            .newLayout     = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .srcStageMask  = vk::PipelineStageFlagBits::eEarlyFragmentTests,
            .dstStageMask  = vk::PipelineStageFlagBits::eEarlyFragmentTests,
            .srcAccessMask = vk::AccessFlagBits::eNone,
            .dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite
        };

        // create a vulkan render pass object
        createVkRenderPass(colorAttachments, depthStencilAttachment);

        createFramebuffer(colorAttachments, depthStencilAttachment);

        // create a graphics pipeline for this render pass
        setupRasterPipeline("shaders/RenderPasses/AccumulationPass/Accum.vert.spv",
                            "shaders/RenderPasses/AccumulationPass/Accum.frag.spv", true);
    }

    void record(vk::CommandBuffer commandBuffer) override {

        std::array<vk::ClearValue, 2> clearValues{};
        clearValues[0].color        = vk::ClearColorValue{ std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f } };
        clearValues[1].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

        vk::RenderPassBeginInfo renderPassInfo{ .renderPass  = m_renderPass,
                                                .framebuffer = m_framebuffer,
                                                .renderArea{ .offset = { 0, 0 }, .extent = m_extent },
                                                .clearValueCount = static_cast<uint32_t>(clearValues.size()),
                                                .pClearValues    = clearValues.data() };

        commandBuffer.beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);

        vk::Viewport viewport{ .x        = 0.0f,
                               .y        = 0.0f,
                               .width    = static_cast<float>(m_extent.width),
                               .height   = static_cast<float>(m_extent.height),
                               .minDepth = 0.0f,
                               .maxDepth = 1.0f };
        commandBuffer.setViewport(0, 1, &viewport);

        vk::Rect2D scissor{ .offset = { 0, 0 }, .extent = m_extent };
        commandBuffer.setScissor(0, 1, &scissor);

        uint32_t objSize         = m_pScene->getObjects().size();
        vk::DeviceSize offsets[] = { 0 };

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, &m_descriptorSet, 0,
                                         nullptr);

        commandBuffer.draw(3, 1, 0, 0);

        commandBuffer.endRenderPass();
    }

private:
    AccumData m_accumData;
    glm::mat4 m_lastCameraView;

}; // class AccumulationPass

} // namespace vuren

#endif // ACCUMULATION_PASS_HPP