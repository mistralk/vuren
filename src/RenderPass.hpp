#ifndef RENDER_PASS_HPP
#define RENDER_PASS_HPP

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#include "Utils.hpp"
#include "Common.hpp"
#include "Texture.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace vrb {

class RasterizationPipeline {
public:
    RasterizationPipeline(VulkanContext* pContext, vk::RenderPass* pRenderPass, vk::DescriptorSetLayout* pDescriptorSetLayout, vk::PipelineLayout* pPipelineLayout, bool blitPass = false)
     : m_pContext(pContext), m_pRenderPass(pRenderPass), m_pDescriptorSetLayout(pDescriptorSetLayout), m_pPipelineLayout(pPipelineLayout), m_blitPass(blitPass) {
    }

    ~RasterizationPipeline() {
    }

    RasterizationPipeline() {}

    void cleanup() {
        m_pContext->m_device.destroyPipeline(m_pipeline, nullptr);
    }

    void setup(const std::string& vertShaderPath, const std::string& fragShaderPath) {
        if (!m_pContext->m_device || !m_pRenderPass || !m_pDescriptorSetLayout || !m_pPipelineLayout) {
            throw std::runtime_error("pipeline setup failed!"
                "logical device, render pass, descriptor set layout and pipeline layout must be valid before the pipeline creation.");
        }

        auto vertShaderCode = readFile(vertShaderPath);
        auto fragShaderCode = readFile(fragShaderPath);

        vk::ShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        vk::ShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo { 
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = vertShaderModule,
            .pName = "main"
        };

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo { 
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = fragShaderModule,
            .pName = "main" 
        };
        
        vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
        
        // Fixed function: Vertex input
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescription = Vertex::getAttributeDescriptions();

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
        if (m_blitPass) {
            vertexInputInfo.vertexBindingDescriptionCount = 0;
            vertexInputInfo.pVertexBindingDescriptions = nullptr;
            vertexInputInfo.vertexAttributeDescriptionCount = 0;
            vertexInputInfo.pVertexAttributeDescriptions = nullptr;
        }
        else {
            vertexInputInfo.vertexBindingDescriptionCount = 1;
            vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
            vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
            vertexInputInfo.pVertexAttributeDescriptions = attributeDescription.data();
        }
        
        // Fixed function: Input assembly
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly { 
            .topology = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = VK_FALSE
        };
        
        // Fixed function: Viewport and scissor        
        std::vector<vk::DynamicState> dynamicStates = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };

        vk::PipelineDynamicStateCreateInfo dynamicState { 
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()
        };

        vk::PipelineViewportStateCreateInfo viewportState {
            .viewportCount = 1,
            .scissorCount = 1
        };

        // Fixed function: Rasterizer
        vk::PipelineRasterizationStateCreateInfo rasterizer { 
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = m_blitPass ? vk::CullModeFlagBits::eNone : vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = VK_FALSE,
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp = 0.0f,
            .depthBiasSlopeFactor = 0.0f,
            .lineWidth = 1.0f
        };
        
        // Fixed function: Multisampling
        vk::PipelineMultisampleStateCreateInfo multisampling { 
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 1.0f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE
        };

        // Fixed function: Depth and stencil testing
        vk::PipelineDepthStencilStateCreateInfo depthStencil { 
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = vk::CompareOp::eLess,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
            .front = {},
            .back = {},
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f
        };

        // Fixed function: Color blending
        vk::PipelineColorBlendAttachmentState colorBlendAttachment { 
            .blendEnable = VK_FALSE,
            .srcColorBlendFactor = vk::BlendFactor::eOne,
            .dstColorBlendFactor = vk::BlendFactor::eZero,
            .colorBlendOp = vk::BlendOp::eAdd,
            .srcAlphaBlendFactor = vk::BlendFactor::eOne,
            .dstAlphaBlendFactor = vk::BlendFactor::eZero,
            .alphaBlendOp = vk::BlendOp::eAdd,
            .colorWriteMask = vk::ColorComponentFlagBits::eR 
                            | vk::ColorComponentFlagBits::eG 
                            | vk::ColorComponentFlagBits::eB 
                            | vk::ColorComponentFlagBits::eA
        };

        vk::PipelineColorBlendStateCreateInfo colorBlending { 
            .logicOpEnable = VK_FALSE,
            .logicOp = vk::LogicOp::eCopy,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment,
            .blendConstants = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}
        };

        vk::GraphicsPipelineCreateInfo pipelineInfo { 
            .stageCount = 2,
            .pStages = shaderStages,
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depthStencil,
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamicState,
            .layout = *m_pPipelineLayout,
            .renderPass = *m_pRenderPass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1
        };

        if (m_pContext->m_device.createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create rasterization pipeline!");
        }

        m_pContext->m_device.destroyShaderModule(fragShaderModule, nullptr);
        m_pContext->m_device.destroyShaderModule(vertShaderModule, nullptr);
    }

    vk::Pipeline getPipeline() {
        return m_pipeline;
    }

    void setRenderPass(vk::RenderPass* pRenderPass) {
        m_pRenderPass = pRenderPass;
    }

    void setPipelineLayout(vk::PipelineLayout* pPipelineLayout) {
        m_pPipelineLayout = pPipelineLayout;
    }

    void setDescriptorSetLayout(vk::DescriptorSetLayout* pDescriptorSetLayout) {
        m_pDescriptorSetLayout = pDescriptorSetLayout;
    }

    void setBlitPass(bool isBlitPass) {
        m_blitPass = isBlitPass;
    }

private:
    vk::ShaderModule createShaderModule(const std::vector<char>& code) {
        vk::ShaderModuleCreateInfo createInfo { 
            .codeSize = code.size(),
            .pCode = reinterpret_cast<const uint32_t*>(code.data())
        };

        vk::ShaderModule shaderModule;
        if (m_pContext->m_device.createShaderModule(&createInfo, nullptr, &shaderModule) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    vk::Pipeline m_pipeline;

    VulkanContext* m_pContext {nullptr};
    vk::RenderPass* m_pRenderPass {nullptr};
    vk::DescriptorSetLayout* m_pDescriptorSetLayout {nullptr};
    vk::PipelineLayout* m_pPipelineLayout {nullptr};

    bool m_blitPass;

}; // class RasterizationPipeline

class VulkanContext;

class RenderPass {
public:
    struct ResourceBindingInfo {
        std::string name;
        vk::DescriptorType descriptorType;
        vk::ShaderStageFlags stageFlags;
    };

    struct AttachmentInfo {
        vk::ImageView imageView;
        vk::Format format;
        vk::ImageLayout oldLayout;
        vk::ImageLayout newLayout;

        vk::PipelineStageFlags srcStageMask;
        vk::PipelineStageFlags dstStageMask;
        vk::AccessFlags srcAccessMask;
        vk::AccessFlags dstAccessMask;
    };

    RenderPass() {}

    RenderPass(VulkanContext* pContext, vk::CommandPool* pCommandPool, std::shared_ptr<std::unordered_map<std::string, Texture>> pGlobalTextureDict, std::shared_ptr<std::unordered_map<std::string, vk::Buffer>> pGlobalBufferDict);
    virtual ~RenderPass();
    
    virtual void setup() = 0;
    virtual void record(vk::CommandBuffer commandBuffer) = 0;
    void cleanup();

    vk::RenderPass getRenderPass() {
        return m_renderPass;
    }

    void setExtent(vk::Extent2D extent) {
        m_extent = extent;
    }

protected:
    RasterizationPipeline m_rasterPipeline;

    vk::RenderPass m_renderPass {VK_NULL_HANDLE};
    vk::Framebuffer m_framebuffer {VK_NULL_HANDLE};
    vk::DescriptorSetLayout m_descriptorSetLayout {VK_NULL_HANDLE};
    vk::PipelineLayout m_pipelineLayout {VK_NULL_HANDLE};
    vk::DescriptorPool m_descriptorPool {VK_NULL_HANDLE};
    vk::DescriptorSet m_descriptorSet {VK_NULL_HANDLE};

    VulkanContext* m_pContext {nullptr};
    vk::CommandPool* m_pCommandPool {nullptr};

    vk::Extent2D m_extent;

    std::shared_ptr<std::unordered_map<std::string, Texture>> m_pGlobalTextureDict {nullptr};
    std::shared_ptr<std::unordered_map<std::string, vk::Buffer>> m_pGlobalBufferDict {nullptr};

    void createDescriptorSet(const std::vector<ResourceBindingInfo>& bindingInfos);
    void createFramebuffer(const std::vector<AttachmentInfo>& colorAttachmentInfos, const AttachmentInfo& depthStencilAttachmentInfo);
    void createVkRenderPass(const std::vector<AttachmentInfo>& colorAttachmentInfos, const AttachmentInfo& depthStencilAttachmentInfo);
    void createRasterPipeline(const std::string& vertShaderPath, const std::string& fragShaderPath, bool isBlitPass = false);

}; // class RenderPass

} // namespace vrb

#endif // RENDER_PASS_HPP