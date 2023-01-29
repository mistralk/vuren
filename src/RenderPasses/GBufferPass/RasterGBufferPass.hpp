#ifndef RASTER_GBUFFER_PASS_HPP
#define RASTER_GBUFFER_PASS_HPP

#include "GBufferCommon.h"
#include "RenderPass.hpp"

namespace vuren {

class RasterGBufferPass : public RasterRenderPass {
public:
    RasterGBufferPass() {}

    ~RasterGBufferPass() {}

    void init(VulkanContext *pContext, vk::CommandPool commandPool, std::shared_ptr<ResourceManager> pResourceManager,
              std::shared_ptr<Scene> pScene) override {
        RasterRenderPass::init(pContext, commandPool, pResourceManager, pScene);
    }

    void updateGui() {
    }

    void define() override {
        // create textures for the attachments
        m_pResourceManager->createTextureRGBA32Sfloat("RasterColor");
        m_pResourceManager->createTextureRGBA32Sfloat("RasterWorldPos");
        m_pResourceManager->createTextureRGBA32Sfloat("RasterWorldNormal");
        m_pResourceManager->createDepthTexture("RasterDepth");

        m_pContext->kOffscreenOutputTextureNames.push_back("RasterColor");
        m_pContext->kOffscreenOutputTextureNames.push_back("RasterWorldPos");
        m_pContext->kOffscreenOutputTextureNames.push_back("RasterWorldNormal");

        // create a descriptor set
        std::vector<ResourceBindingInfo> bindings;

        // uniform buffers
        bindings.push_back({ "CameraBuffer", vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1 });
        // model textures
        bindings.push_back({ "SceneTextures", vk::DescriptorType::eCombinedImageSampler,
                             vk::ShaderStageFlagBits::eFragment,
                             static_cast<uint32_t>(m_pScene->getTextures().size()) });

        createDescriptorSet(bindings);

        // create framebuffers for the attachments
        std::vector<AttachmentInfo> colorAttachments = {
            { .imageView     = m_pResourceManager->getTexture("RasterColor")->descriptorInfo.imageView,
              .format        = vk::Format::eR32G32B32A32Sfloat,
              .oldLayout     = vk::ImageLayout::eUndefined,
              .newLayout     = vk::ImageLayout::eColorAttachmentOptimal,
              .srcStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput,
              .dstStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput,
              .srcAccessMask = vk::AccessFlagBits::eNone,
              .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite },
            { .imageView     = m_pResourceManager->getTexture("RasterWorldPos")->descriptorInfo.imageView,
              .format        = vk::Format::eR32G32B32A32Sfloat,
              .oldLayout     = vk::ImageLayout::eUndefined,
              .newLayout     = vk::ImageLayout::eColorAttachmentOptimal,
              .srcStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput,
              .dstStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput,
              .srcAccessMask = vk::AccessFlagBits::eNone,
              .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite },
            { .imageView     = m_pResourceManager->getTexture("RasterWorldNormal")->descriptorInfo.imageView,
              .format        = vk::Format::eR32G32B32A32Sfloat,
              .oldLayout     = vk::ImageLayout::eUndefined,
              .newLayout     = vk::ImageLayout::eColorAttachmentOptimal,
              .srcStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput,
              .dstStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput,
              .srcAccessMask = vk::AccessFlagBits::eNone,
              .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite }
        };

        AttachmentInfo depthStencilAttachment = {
            .imageView     = m_pResourceManager->getTexture("RasterDepth")->descriptorInfo.imageView,
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
        setupRasterPipeline("shaders/RenderPasses/GBufferPass/RasterGBuffer.vert.spv",
                            "shaders/RenderPasses/GBufferPass/RasterGBuffer.frag.spv");
    }

    void outputTextureBarrier(vk::CommandBuffer commandBuffer) override {
        auto colorTexture = m_pResourceManager->getTexture("RasterColor");
        transitionImageLayout(commandBuffer, colorTexture, vk::ImageLayout::eColorAttachmentOptimal,
                              vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits::eAllGraphics,
                              vk::PipelineStageFlagBits::eAllGraphics);

        auto posTexture = m_pResourceManager->getTexture("RasterWorldPos");
        transitionImageLayout(commandBuffer, posTexture, vk::ImageLayout::eColorAttachmentOptimal,
                              vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits::eAllGraphics,
                              vk::PipelineStageFlagBits::eAllGraphics);

        auto normalTexture = m_pResourceManager->getTexture("RasterWorldNormal");
        transitionImageLayout(commandBuffer, normalTexture, vk::ImageLayout::eColorAttachmentOptimal,
                              vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits::eAllGraphics,
                              vk::PipelineStageFlagBits::eAllGraphics);
    }

    void record(vk::CommandBuffer commandBuffer) override {
        std::array<vk::ClearValue, 4> clearValues{};
        clearValues[0].color        = vk::ClearColorValue{ std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f } };
        clearValues[1].color        = vk::ClearColorValue{ std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f } };
        clearValues[2].color        = vk::ClearColorValue{ std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f } };
        clearValues[3].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

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

        for (uint32_t i = 0; i < objSize; ++i) {
            auto object              = m_pScene->getObject(i);
            vk::Buffer vertexBuffers = object.pVertexBuffer->descriptorInfo.buffer;
            vk::Buffer instanceBuffer =
                m_pResourceManager->getBuffer("InstanceBuffer" + std::to_string(i))->descriptorInfo.buffer;

            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, &m_descriptorSet,
                                             0, nullptr);

            commandBuffer.bindVertexBuffers(0, 1, &vertexBuffers, offsets);
            commandBuffer.bindVertexBuffers(1, 1, &instanceBuffer, offsets);
            commandBuffer.bindIndexBuffer(object.pIndexBuffer->descriptorInfo.buffer, 0, vk::IndexType::eUint32);

            commandBuffer.drawIndexed(object.indexBufferSize, object.instanceCount, 0, 0, 0);
        }

        commandBuffer.endRenderPass();
    }

private:
    vk::Format m_colorAttachmentFormat{ vk::Format::eR32G32B32A32Sfloat };

}; // class RasterGBufferPass

} // namespace vuren

#endif // RASTER_GBUFFER_PASS_HPP