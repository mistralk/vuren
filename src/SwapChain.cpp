#include <imgui/backends/imgui_impl_vulkan.h>

#include "SwapChain.hpp"

namespace vuren {

// ------------------ FinalRenderPass class ------------------

void FinalRenderPass::define() {
    assert(m_pSwapChain != nullptr);

    // create the attachment images
    // global dict is not required for swap chain images
    m_pResourceManager->createDepthTexture("FinalDepth");

    // create a descriptor set
    std::vector<ResourceBindingInfo> bindings = { { m_pContext->kOffscreenOutputTextureNames[m_pContext->kCurrentItem],
                                                    vk::DescriptorType::eCombinedImageSampler,
                                                    vk::ShaderStageFlagBits::eFragment, 1 } };
    createDescriptorSet(bindings);

    // create the renderpass
    std::vector<AttachmentInfo> colorAttachments;
    AttachmentInfo colorAttachment{ .imageView     = VK_NULL_HANDLE,
                                    .format        = vk::Format::eB8G8R8A8Srgb,
                                    .oldLayout     = vk::ImageLayout::eUndefined,
                                    .newLayout     = vk::ImageLayout::ePresentSrcKHR,
                                    .srcStageMask  = vk::PipelineStageFlagBits::eBottomOfPipe,
                                    .dstStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                    .srcAccessMask = vk::AccessFlagBits::eMemoryRead,
                                    .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                                                     vk::AccessFlagBits::eColorAttachmentRead };
    colorAttachments.push_back(colorAttachment);
    AttachmentInfo depthStencilAttachment = {
        .imageView     = m_pResourceManager->getTexture("FinalDepth")->descriptorInfo.imageView,
        .format        = findDepthFormat(*m_pContext),
        .oldLayout     = vk::ImageLayout::eUndefined,
        .newLayout     = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        .srcStageMask  = vk::PipelineStageFlagBits::eNone,
        .dstStageMask  = vk::PipelineStageFlagBits::eNone,
        .srcAccessMask = vk::AccessFlagBits::eNone,
        .dstAccessMask = vk::AccessFlagBits::eNone
    };
    createVkRenderPass(colorAttachments, depthStencilAttachment);

    // create swap chain framebuffers
    createSwapChainFrameBuffers(*m_pResourceManager->getTexture("FinalDepth"));

    // create a graphics pipeline for this render pass
    setupRasterPipeline("shaders/CommonShaders/Final.vert.spv", "shaders/CommonShaders/Final.frag.spv", true);
}

void FinalRenderPass::record(vk::CommandBuffer commandBuffer) {
    std::array<vk::ClearValue, 2> clearValues{};
    clearValues[0].color        = vk::ClearColorValue{ std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f } };
    clearValues[1].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

    vk::RenderPassBeginInfo renderPassInfo{
        .renderPass  = m_renderPass,
        .framebuffer = (*m_pSwapChain->getSwapChainFrameBuffers())[m_pSwapChain->getImageIndex()],
        .renderArea{ .offset = { 0, 0 }, .extent = m_extent },
        .clearValueCount = static_cast<uint32_t>(clearValues.size()),
        .pClearValues    = clearValues.data()
    };

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

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, &m_descriptorSet, 0,
                                     nullptr);

    commandBuffer.draw(3, 1, 0, 0);

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

    commandBuffer.endRenderPass();
}

void FinalRenderPass::createSwapChainFrameBuffers(Texture swapChainDepthImage) {
    m_pSwapChain->createFrameBuffers(m_renderPass, swapChainDepthImage);
    m_colorAttachmentCount = 1;
}

void FinalRenderPass::updateDescriptorSets() {
    std::vector<ResourceBindingInfo> bindings = { { m_pContext->kOffscreenOutputTextureNames[m_pContext->kCurrentItem],
                                                    vk::DescriptorType::eCombinedImageSampler,
                                                    vk::ShaderStageFlagBits::eFragment } };

    std::vector<vk::WriteDescriptorSet> descriptorWrites;
    std::vector<vk::DescriptorImageInfo> imageInfos;
    imageInfos.reserve(bindings.size());

    for (size_t i = 0; i < bindings.size(); ++i) {
        vk::DescriptorImageInfo imageInfo;

        switch (bindings[i].descriptorType) {
            case vk::DescriptorType::eCombinedImageSampler:
                imageInfo.sampler     = m_pResourceManager->getTexture(bindings[i].name)->descriptorInfo.sampler;
                imageInfo.imageView   = m_pResourceManager->getTexture(bindings[i].name)->descriptorInfo.imageView;
                imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                imageInfos.emplace_back(imageInfo);
                break;

            default:
                throw std::runtime_error("unsupported descriptor type!");
        }

        vk::WriteDescriptorSet bufferWrite = { .dstSet           = m_descriptorSet,
                                               .dstBinding       = static_cast<uint32_t>(i),
                                               .dstArrayElement  = 0,
                                               .descriptorCount  = 1,
                                               .descriptorType   = bindings[i].descriptorType,
                                               .pImageInfo       = &imageInfos.back(),
                                               .pBufferInfo      = nullptr,
                                               .pTexelBufferView = nullptr };

        descriptorWrites.emplace_back(bufferWrite);
    }

    m_pContext->m_device.updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(),
                                              0, nullptr);
}

// ------------------ SwapChain class ------------------

void SwapChain::cleanupSwapChain() {
    for (size_t i = 0; i < m_swapChainFramebuffers->size(); ++i) {
        m_pContext->m_device.destroyFramebuffer((*m_swapChainFramebuffers)[i], nullptr);
    }

    for (size_t i = 0; i < m_swapChainColorImageViews->size(); ++i) {
        m_pContext->m_device.destroyImageView((*m_swapChainColorImageViews)[i], nullptr);
    }

    m_pContext->m_device.destroySwapchainKHR(m_swapChain, nullptr);
}

void SwapChain::createSwapChain() {
    m_swapChainFramebuffers    = std::make_shared<std::vector<vk::Framebuffer>>();
    m_swapChainColorImages     = std::make_shared<std::vector<vk::Image>>();
    m_swapChainColorImageViews = std::make_shared<std::vector<vk::ImageView>>();

    SwapChainSupportDetails swapChainSupport = m_pContext->querySwapChainSupport(m_pContext->m_physicalDevice);

    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    vk::PresentModeKHR presentMode     = chooseSwapPresentMode(swapChainSupport.presentModes);
    vk::Extent2D extent                = chooseSwapExtent(swapChainSupport.capabilities);

    m_imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && m_imageCount > swapChainSupport.capabilities.maxImageCount) {
        m_imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo{ .surface          = m_pContext->m_surface,
                                           .minImageCount    = m_imageCount,
                                           .imageFormat      = surfaceFormat.format,
                                           .imageColorSpace  = surfaceFormat.colorSpace,
                                           .imageExtent      = extent,
                                           .imageArrayLayers = 1,
                                           .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment };

    QueueFamilyIndices indices    = m_pContext->findQueueFamilies(m_pContext->m_physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode      = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices   = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode      = vk::SharingMode::eExclusive;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices   = nullptr;
    }

    createInfo.preTransform   = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode    = presentMode;
    createInfo.clipped        = VK_TRUE;
    createInfo.oldSwapchain   = VK_NULL_HANDLE;

    if (m_pContext->m_device.createSwapchainKHR(&createInfo, nullptr, &m_swapChain) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create swap chain!");
    }

    if (m_pContext->m_device.getSwapchainImagesKHR(m_swapChain, &m_imageCount, nullptr) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to get swap chain imageCount!");
    }
    m_swapChainColorImages->resize(m_imageCount);
    if (m_pContext->m_device.getSwapchainImagesKHR(m_swapChain, &m_imageCount, m_swapChainColorImages->data()) !=
        vk::Result::eSuccess) {
        throw std::runtime_error("failed to get swap chain!");
    }
    m_imageFormat = surfaceFormat.format;
    m_extent      = extent;
}

void SwapChain::createSwapChainImageViews() {
    m_swapChainColorImageViews->resize(m_swapChainColorImages->size());

    for (size_t i = 0; i < m_swapChainColorImageViews->size(); ++i) {
        vk::ImageViewCreateInfo viewInfo{ .image            = (*m_swapChainColorImages)[i],
                                          .viewType         = vk::ImageViewType::e2D,
                                          .format           = m_imageFormat,
                                          .subresourceRange = { .aspectMask     = vk::ImageAspectFlagBits::eColor,
                                                                .baseMipLevel   = 0,
                                                                .levelCount     = 1,
                                                                .baseArrayLayer = 0,
                                                                .layerCount     = 1 } };

        if (m_pContext->m_device.createImageView(&viewInfo, nullptr, &(*m_swapChainColorImageViews)[i]) !=
            vk::Result::eSuccess) {
            throw std::runtime_error("failed to create swap chain image view!");
        }
    }
}

void SwapChain::createFrameBuffers(vk::RenderPass renderPass, Texture swapChainDepthImage) {
    m_swapChainFramebuffers->resize(m_swapChainColorImageViews->size());

    for (size_t i = 0; i < m_swapChainFramebuffers->size(); ++i) {
        // Color attachment differs for every swap chain image,
        // but the same depth image can be used by all of them
        // because only a single subpass is running at the same time due to out semaphores
        std::array<vk::ImageView, 2> attachments = { (*m_swapChainColorImageViews)[i],
                                                     swapChainDepthImage.descriptorInfo.imageView };

        vk::FramebufferCreateInfo framebufferInfo{ .renderPass      = renderPass,
                                                   .attachmentCount = static_cast<uint32_t>(attachments.size()),
                                                   .pAttachments    = attachments.data(),
                                                   .width           = m_extent.width,
                                                   .height          = m_extent.height,
                                                   .layers          = 1 };

        if (m_pContext->m_device.createFramebuffer(&framebufferInfo, nullptr, &(*m_swapChainFramebuffers)[i]) !=
            vk::Result::eSuccess) {
            throw std::runtime_error("failed to create swapchain framebuffer!");
        }
    }
}

vk::SurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
    for (const auto &availableFormat: availableFormats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
            availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

vk::PresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes) {
    for (const auto &availablePresentMode: availablePresentModes) {
        if (availablePresentMode == vk::PresentModeKHR::eImmediate) {
            return availablePresentMode;
        }
    }
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D SwapChain::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(m_pWindow, &width, &height);

        vk::Extent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

        actualExtent.width =
            std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height =
            std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}

// currently not properly working
void SwapChain::recreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_pWindow, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(m_pWindow, &width, &height);
        glfwWaitEvents();
    }

    m_pContext->m_device.waitIdle();

    // cleanupSwapChain();
    // createSwapChain();
    // createSwapChainImageViews();
    // createSwapChainFrameBuffers();
    // updateFinalDescriptorSets();
}

} // namespace vuren