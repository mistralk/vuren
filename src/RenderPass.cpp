#include "RenderPass.hpp"
#include "VulkanContext.hpp"

namespace vrb {

RenderPass::RenderPass(VulkanContext* pContext, vk::CommandPool* pCommandPool, std::shared_ptr<ResourceManager> pResourceManager, std::shared_ptr<Scene> pScene)
    : m_pContext(pContext),
      m_pCommandPool(pCommandPool),
      m_pResourceManager(pResourceManager), 
      m_pScene(pScene),
      m_rasterPipeline(pContext, &m_renderPass, &m_descriptorSetLayout, &m_pipelineLayout, false) {
}

RenderPass::~RenderPass() {
}

void RenderPass::createVkRenderPass(const std::vector<AttachmentInfo>& colorAttachmentInfos, const AttachmentInfo& depthStencilAttachmentInfo) {
    if (m_renderPass) {
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
        
        // TODO: how to deal with PresentSrcKHR layout for final render pass?
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
        .colorAttachmentCount = 1,
        .pColorAttachments = colorAttachmentRefs.data(),
        .pDepthStencilAttachment = &depthAttachmentRef 
    };
    
    vk::SubpassDependency dependency { 
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = srcStageMask,
        .dstStageMask = dstStageMask,
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = dstAccessMask
    };

    vk::RenderPassCreateInfo renderPassInfo { 
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency 
    };
    
    if (m_pContext->m_device.createRenderPass(&renderPassInfo, nullptr, &m_renderPass) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void RenderPass::cleanup() {
    if (m_framebuffer) m_pContext->m_device.destroyFramebuffer(m_framebuffer, nullptr);
    m_pContext->m_device.destroyRenderPass(m_renderPass, nullptr);
    m_pContext->m_device.destroyDescriptorPool(m_descriptorPool, nullptr);
    m_pContext->m_device.destroyDescriptorSetLayout(m_descriptorSetLayout, nullptr);
    m_pContext->m_device.destroyPipelineLayout(m_pipelineLayout, nullptr);
    m_rasterPipeline.cleanup();
}

void RenderPass::createFramebuffer(const std::vector<AttachmentInfo>& colorAttachmentInfos, const AttachmentInfo& depthStencilAttachmentInfo) {
    std::vector<vk::ImageView> attachments;
    for (const auto& attachment : colorAttachmentInfos) {
        attachments.push_back(attachment.imageView);
    }
    attachments.push_back(depthStencilAttachmentInfo.imageView);

    vk::FramebufferCreateInfo framebufferInfo { 
        .renderPass = m_renderPass,
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .width = m_extent.width,
        .height = m_extent.height,
        .layers = 1
    };

    if (m_pContext->m_device.createFramebuffer(&framebufferInfo, nullptr, &m_framebuffer) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create framebuffer!");
    }
}

void RenderPass::createDescriptorSet(const std::vector<ResourceBindingInfo>& bindingInfos) {
    std::vector<vk::DescriptorSetLayoutBinding> bindings;

    // create a descriptor set
    for (const auto& binding : bindingInfos) {
        vk::DescriptorSetLayoutBinding layoutBinding { 
            .binding = static_cast<uint32_t>(bindings.size()),
            .descriptorType = binding.descriptorType,
            .descriptorCount = 1,
            .stageFlags = binding.stageFlags,
            .pImmutableSamplers = nullptr
        };

        bindings.push_back(layoutBinding);
    }

    vk::DescriptorSetLayoutCreateInfo layoutInfo { 
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };
    
    if (m_pContext->m_device.createDescriptorSetLayout(&layoutInfo, nullptr, &m_descriptorSetLayout) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create a descriptor set layout!");
    }

    // create a pipeline layout
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo { 
        .setLayoutCount = 1,
        .pSetLayouts = &m_descriptorSetLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };

    if (m_pContext->m_device.createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_pipelineLayout) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create a pipeline layout!");
    }

    // create a descriptor pool
    std::vector<vk::DescriptorPoolSize> poolSizes(bindings.size());
    for (size_t i = 0; i < poolSizes.size(); ++i) {
        poolSizes[i].type = bindings[i].descriptorType;
        poolSizes[i].descriptorCount = 1;
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

    // TO FIX: weird vectors... do i really need it?
    std::vector<vk::DescriptorImageInfo> tempImageInfos; 
    std::vector<vk::DescriptorBufferInfo> tempBufferInfos; 
    tempImageInfos.resize(bindings.size());
    tempBufferInfos.resize(bindings.size());

    std::vector<vk::WriteDescriptorSet> descriptorWrites;

    for (size_t i = 0; i < bindings.size(); ++i) {
        vk::DescriptorBufferInfo* pBufferInfo = nullptr;
        vk::DescriptorImageInfo* pImageInfo = nullptr;

        switch (bindings[i].descriptorType) {
            case vk::DescriptorType::eCombinedImageSampler:
                tempImageInfos[i].sampler = m_pResourceManager->getTexture(bindingInfos[i].name).descriptorInfo.sampler;
                tempImageInfos[i].imageView = m_pResourceManager->getTexture(bindingInfos[i].name).descriptorInfo.imageView;
                tempImageInfos[i].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                pImageInfo = &tempImageInfos[i];
                break;
            
            case vk::DescriptorType::eUniformBuffer:
                tempBufferInfos[i].buffer = m_pResourceManager->getBuffer(bindingInfos[i].name).descriptorInfo.buffer;
                tempBufferInfos[i].offset = 0;
                tempBufferInfos[i].range = sizeof(UniformBufferObject);
                pBufferInfo = &tempBufferInfos[i];
                break;

            default:
                throw std::runtime_error("unsupported descriptor type!"); 
        }

        vk::WriteDescriptorSet bufferWrite = { 
            .dstSet = m_descriptorSet,
            .dstBinding = static_cast<uint32_t>(i),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = bindings[i].descriptorType,
            .pImageInfo = pImageInfo,
            .pBufferInfo = pBufferInfo,
            .pTexelBufferView = nullptr
        };

        descriptorWrites.push_back(bufferWrite);
    }
    
    m_pContext->m_device.updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void RenderPass::createRasterPipeline(const std::string& vertShaderPath, const std::string& fragShaderPath, bool isBlitPass) {
    m_rasterPipeline.setRenderPass(&m_renderPass);
    m_rasterPipeline.setPipelineLayout(&m_pipelineLayout);
    m_rasterPipeline.setDescriptorSetLayout(&m_descriptorSetLayout);
    m_rasterPipeline.setBlitPass(isBlitPass);
    m_rasterPipeline.setup(vertShaderPath, fragShaderPath);
}

}; // namespace vrb