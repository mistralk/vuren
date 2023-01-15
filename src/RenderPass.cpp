#include "RenderPass.hpp"
#include "VulkanContext.hpp"

namespace vrb {

void RenderPass::createVkRenderPass(const std::vector<AttachmentInfo>& colorAttachmentInfos, const AttachmentInfo& depthStencilAttachmentInfo) {
    if (m_rasterProperties.renderPass) {
        return;
    }

    std::vector<vk::AttachmentDescription> attachments; 
    std::vector<vk::AttachmentReference> colorAttachmentRefs; 
    vk::PipelineStageFlags srcStageMask = vk::PipelineStageFlagBits::eNone;
    vk::PipelineStageFlags dstStageMask = vk::PipelineStageFlagBits::eNone;
    vk::AccessFlags srcAccessMask = vk::AccessFlagBits::eNone;
    vk::AccessFlags dstAccessMask = vk::AccessFlagBits::eNone;

    // color attachments
    for (const auto& attachmentInfo : colorAttachmentInfos) {
        vk::AttachmentDescription colorAttachment { 
            .format = attachmentInfo.format, 
            .samples = vk::SampleCountFlagBits::e1,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = attachmentInfo.oldLayout,
            .finalLayout = attachmentInfo.newLayout
        };
        
        vk::AttachmentReference colorAttachmentRef { 
            .attachment = static_cast<uint32_t>(attachments.size()),
            .layout = attachmentInfo.newLayout == vk::ImageLayout::ePresentSrcKHR ? vk::ImageLayout::eColorAttachmentOptimal : attachmentInfo.newLayout
        };

        srcStageMask |= attachmentInfo.srcStageMask;
        dstStageMask |= attachmentInfo.dstStageMask;
        srcAccessMask |= attachmentInfo.srcAccessMask;
        dstAccessMask |= attachmentInfo.dstAccessMask;

        attachments.push_back(colorAttachment);
        colorAttachmentRefs.push_back(colorAttachmentRef);
    }
    
    // depth stencil attachment
    vk::AttachmentDescription depthAttachment { 
        .format = depthStencilAttachmentInfo.format,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eDontCare,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = depthStencilAttachmentInfo.oldLayout,
        .finalLayout = depthStencilAttachmentInfo.newLayout
    };

    vk::AttachmentReference depthAttachmentRef { 
        .attachment = static_cast<uint32_t>(attachments.size()),
        .layout = depthStencilAttachmentInfo.newLayout
    };
    
    attachments.push_back(depthAttachment);

    vk::SubpassDescription subpass = { 
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size()),
        .pColorAttachments = colorAttachmentRefs.data(),
        .pDepthStencilAttachment = &depthAttachmentRef 
    };
    
    // vk::SubpassDependency dependency { 
    //     .srcSubpass = VK_SUBPASS_EXTERNAL,
    //     .dstSubpass = 0,
    //     .srcStageMask = srcStageMask,
    //     .dstStageMask = dstStageMask,
    //     .srcAccessMask = srcAccessMask,
    //     .dstAccessMask = dstAccessMask
    // };

    vk::SubpassDependency dependency { 
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eFragmentShader,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits::eShaderRead,
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite
    };

    vk::RenderPassCreateInfo renderPassInfo { 
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency 
    };
    
    if (m_pContext->m_device.createRenderPass(&renderPassInfo, nullptr, &m_rasterProperties.renderPass) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void RenderPass::cleanup() {
    if (m_rasterProperties.framebuffer) m_pContext->m_device.destroyFramebuffer(m_rasterProperties.framebuffer, nullptr);
    if (m_rasterProperties.renderPass) m_pContext->m_device.destroyRenderPass(m_rasterProperties.renderPass, nullptr);
    if (m_descriptorPool) m_pContext->m_device.destroyDescriptorPool(m_descriptorPool, nullptr);
    if (m_descriptorSetLayout) m_pContext->m_device.destroyDescriptorSetLayout(m_descriptorSetLayout, nullptr);
    m_pPipeline->cleanup();
}

void RenderPass::createFramebuffer(const std::vector<AttachmentInfo>& colorAttachmentInfos, const AttachmentInfo& depthStencilAttachmentInfo) {
    std::vector<vk::ImageView> attachments;
    for (const auto& attachment : colorAttachmentInfos) {
        attachments.push_back(attachment.imageView);
    }
    attachments.push_back(depthStencilAttachmentInfo.imageView);

    vk::FramebufferCreateInfo framebufferInfo { 
        .renderPass = m_rasterProperties.renderPass,
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .width = m_extent.width,
        .height = m_extent.height,
        .layers = 1
    };

    if (m_pContext->m_device.createFramebuffer(&framebufferInfo, nullptr, &m_rasterProperties.framebuffer) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create framebuffer!");
    }

    m_rasterProperties.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentInfos.size());
}

void RenderPass::createDescriptorSet(const std::vector<ResourceBindingInfo>& bindingInfos) {
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    uint32_t totalDescriptorCount = 0;

    // create a descriptor set
    for (const auto& binding : bindingInfos) {
        vk::DescriptorSetLayoutBinding layoutBinding { 
            .binding = static_cast<uint32_t>(bindings.size()),
            .descriptorType = binding.descriptorType,
            .descriptorCount = binding.descriptorCount,
            .stageFlags = binding.stageFlags,
            .pImmutableSamplers = nullptr
        };

        bindings.push_back(layoutBinding);

        totalDescriptorCount += binding.descriptorCount;
    }

    vk::DescriptorSetLayoutCreateInfo layoutInfo { 
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };
    
    if (m_pContext->m_device.createDescriptorSetLayout(&layoutInfo, nullptr, &m_descriptorSetLayout) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create a descriptor set layout!");
    }

    // create a descriptor pool
    std::vector<vk::DescriptorPoolSize> poolSizes(bindings.size());
    for (size_t i = 0; i < poolSizes.size(); ++i) {
        poolSizes[i].type = bindings[i].descriptorType;
        poolSizes[i].descriptorCount = bindings[i].descriptorCount;
    }        
    
    vk::DescriptorPoolCreateInfo poolInfo { 
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    if (m_pContext->m_device.createDescriptorPool(&poolInfo, nullptr, &m_descriptorPool) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create a descriptor pool!");
    }

    // write a descriptor set
    vk::DescriptorSetAllocateInfo allocInfo { 
        .descriptorPool = m_descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_descriptorSetLayout
    };
    
    if (m_pContext->m_device.allocateDescriptorSets(&allocInfo, &m_descriptorSet) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to allocate offscreen descriptor sets!");
    }

    std::vector<vk::WriteDescriptorSet> descriptorWrites;
    std::vector<vk::DescriptorBufferInfo> bufferInfos;
    std::vector<vk::DescriptorImageInfo> imageInfos;

    bufferInfos.reserve(totalDescriptorCount);
    imageInfos.reserve(totalDescriptorCount);

    for (size_t i = 0; i < bindings.size(); ++i) {
        vk::WriteDescriptorSet write;
        vk::DescriptorBufferInfo bufferInfo;
        vk::DescriptorImageInfo imageInfo;

        write.pImageInfo = nullptr;
        write.pBufferInfo = nullptr;

        if (bindings[i].descriptorType == vk::DescriptorType::eAccelerationStructureKHR) {
            vk::AccelerationStructureKHR tlas = m_rayTracingProperties.tlas.as;
            vk::WriteDescriptorSetAccelerationStructureKHR descriptorSetAsInfo {
                .accelerationStructureCount = 1,
                .pAccelerationStructures = &tlas
            };
            
            write.pNext = (void*)&descriptorSetAsInfo;
            write.dstSet = m_descriptorSet;
            write.dstBinding = static_cast<uint32_t>(i);
            write.dstArrayElement = 0;
            write.descriptorCount = 1;
            write.descriptorType = bindings[i].descriptorType;
            write.pTexelBufferView = nullptr;
        }

        else {
            switch (bindings[i].descriptorType) {
                case vk::DescriptorType::eCombinedImageSampler:
                    if (bindingInfos[i].name == "SceneTextures") {
                        for (auto& texture : m_pScene->getTextures()) {
                            imageInfo = m_pResourceManager->getTexture(texture.name).descriptorInfo;
                            imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                            imageInfos.push_back(imageInfo);
                        }
                        write.pImageInfo = &imageInfos.back() - m_pScene->getTextures().size() + 1;
                    }
                    else {
                        imageInfo = m_pResourceManager->getTexture(bindingInfos[i].name).descriptorInfo;
                        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                        imageInfos.push_back(imageInfo);
                        write.pImageInfo = &imageInfos.back();
                    }
                    break;
                
                case vk::DescriptorType::eStorageImage:
                    imageInfo = m_pResourceManager->getTexture(bindingInfos[i].name).descriptorInfo;
                    imageInfo.imageLayout = vk::ImageLayout::eGeneral;
                    imageInfos.push_back(imageInfo);
                    write.pImageInfo = &imageInfos.back();
                    break;
                
                case vk::DescriptorType::eUniformBuffer:
                    bufferInfo = m_pResourceManager->getBuffer(bindingInfos[i].name).descriptorInfo;
                    bufferInfos.push_back(bufferInfo);
                    write.pBufferInfo = &bufferInfos.back();
                    break;

                case vk::DescriptorType::eStorageBuffer:
                    bufferInfo = m_pResourceManager->getBuffer(bindingInfos[i].name).descriptorInfo;
                    bufferInfos.push_back(bufferInfo);
                    write.pBufferInfo = &bufferInfos.back();
                    break;

                default:
                    throw std::runtime_error("unsupported descriptor type!"); 
            }

            write.dstSet = m_descriptorSet;
            write.dstBinding = static_cast<uint32_t>(i);
            write.dstArrayElement = 0;
            write.descriptorCount = bindings[i].descriptorCount;
            write.descriptorType = bindings[i].descriptorType;
            write.pTexelBufferView = nullptr;
        }

        descriptorWrites.push_back(write);
    }
    
    m_pContext->m_device.updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void RenderPass::setupRasterPipeline(const std::string& vertShaderPath, const std::string& fragShaderPath, bool isBlitPass) {
    assert(m_pipelineType == PipelineType::eRasterization);

    m_rasterProperties.vertShaderPath = vertShaderPath;
    m_rasterProperties.fragShaderPath = fragShaderPath;
    m_rasterProperties.isBiltPass = isBlitPass;

    m_pPipeline = std::make_unique<RasterizationPipeline>(m_pContext, m_descriptorSetLayout, m_rasterProperties);

    m_pPipeline->setup();
}

void RenderPass::setupRayTracingPipeline(const std::string& raygenShaderPath, const std::string& missShaderPath, const std::string& closestHitShaderPath) {
    assert(m_pipelineType == PipelineType::eRayTracing);

    m_rayTracingProperties.raygenShaderPath = raygenShaderPath;
    m_rayTracingProperties.missShaderPath = missShaderPath;
    m_rayTracingProperties.closestHitShaderPath = closestHitShaderPath;

    m_pPipeline = std::make_unique<RayTracingPipeline>(m_pContext, m_descriptorSetLayout, m_rayTracingProperties);

    m_pPipeline->setup();
}

}; // namespace vrb