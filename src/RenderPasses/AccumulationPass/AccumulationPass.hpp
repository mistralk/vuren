#ifndef ACCUMULATION_PASS_HPP
#define ACCUMULATION_PASS_HPP

#include "RenderPass.hpp"

namespace vuren {
class AccumulationPass : public RasterRenderPass {
public:
    AccumulationPass() {}

    ~AccumulationPass() {}

    void init(VulkanContext *pContext, vk::CommandPool commandPool, std::shared_ptr<ResourceManager> pResourceManager,
              std::shared_ptr<Scene> pScene) override {
        RasterRenderPass::init(pContext, commandPool, pResourceManager, pScene);
    }

    void updateGui() {
        if (!ImGui::CollapsingHeader("Accumulation Pass"))
            return;
    }

    void define() override {
        m_pResourceManager->createTextureRGBA32Sfloat("CurrentFrame");
        m_pResourceManager->createTextureRGBA32Sfloat("HistoryBuffer");
        m_pResourceManager->createDepthTexture("AccumDepth");

        // create a descriptor set
        std::vector<ResourceBindingInfo> bindings;
        bindings.push_back(
            { "CurrentFrame", vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1 });
        bindings.push_back(
            { "HistoryBuffer", vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1 });

        createDescriptorSet(bindings);

        // create framebuffers for the attachments
        std::vector<AttachmentInfo> colorAttachments = {
            { .imageView     = m_pResourceManager->getTexture("HistoryBuffer").descriptorInfo.imageView,
              .format        = vk::Format::eR32G32B32A32Sfloat,
              .oldLayout     = vk::ImageLayout::eUndefined,
              .newLayout     = vk::ImageLayout::eColorAttachmentOptimal,
              .srcStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput,
              .dstStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput,
              .srcAccessMask = vk::AccessFlagBits::eNone,
              .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite }
        };

        AttachmentInfo depthStencilAttachment = {
            .imageView     = m_pResourceManager->getTexture("AccumDepth").descriptorInfo.imageView,
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
                            "shaders/RenderPasses/AccumulationPass/Accum.frag.spv");
    }

    void record(vk::CommandBuffer commandBuffer) override {
        
        // 1. copy a texture(a previous renderpass' output) to the current frame buffer
        // 이 인풋/아웃풋 관계를 어떻게 해당 렌더패스 바깥에서 정의할 수 있을까? 
        // 인풋이 여러개일수도 있다.
        // 아웃풋도 여러개일수도 있다.
        // 한 패스의 인풋은 기본적으로 텍스쳐 이름을 통해서 define() 함수에서 정의된다. define()을 직접 수정하지 않고, 렌더그래프 레벨에서 인풋 텍스쳐를 설정해주려면?
        // 어떤 아웃풋과 어떤 인풋을 매칭시켜줘야 한다
        // 이걸 실제 카피를 통해서 하는건 너무 비효율적이지 않을까?
        // "이름은 다르지만"(예: 이전패스에서는 RGBcolor, 이번패스에서는 CurrentColor) 동일한 텍스쳐를 가리키도록 설정할 수 있지 않을까?
        // connectRenderPass(input key, output key)
        // 이러면 input key와 output key는 동일한 텍스쳐를 레퍼런스하게 된다
        // define 처음에 create texture할때, 해당 텍스쳐가 존재하는 경우 그대로 연결시키고, 없는경우 새로 만들면 될 것
        // 삭제할때는?

        // 2. accumulate the history buffer in shader

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

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, &m_descriptorSet, 0,
                                         nullptr);

        commandBuffer.draw(3, 1, 0, 0);

        commandBuffer.endRenderPass();
    }

private:
    uint32_t m_accumulatedCount{ 0 };

}; // class AccumulationPass

} // namespace vuren

#endif // ACCUMULATION_PASS_HPP