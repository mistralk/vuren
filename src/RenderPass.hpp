#ifndef RENDER_PASS_HPP
#define RENDER_PASS_HPP

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#include "Utils.hpp"
#include "Common.hpp"
#include "Scene.hpp"
#include "ResourceManager.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace vrb {

class RasterizationPipeline {
public:
    RasterizationPipeline(VulkanContext* pContext, vk::RenderPass renderPass, vk::DescriptorSetLayout descriptorSetLayout, vk::PipelineLayout pipelineLayout, bool blitPass = false)
     : m_pContext(pContext), m_renderPass(renderPass), m_descriptorSetLayout(descriptorSetLayout), m_pipelineLayout(pipelineLayout), m_blitPass(blitPass) {
    }

    ~RasterizationPipeline() {
    }

    RasterizationPipeline() {}

    void cleanup() {
        m_pContext->m_device.destroyPipeline(m_pipeline, nullptr);
    }

    void setup(const std::string& vertShaderPath, const std::string& fragShaderPath) {
        if (!m_pContext->m_device || !m_renderPass || !m_descriptorSetLayout || !m_pipelineLayout) {
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
        // per-vertex data
        auto vertexAttributeDescription = Vertex::getAttributeDescriptions();
        
        // per-instance data
        auto instanceAttributeDescription = ObjectInstance::getAttributeDescriptions();

        std::vector<vk::VertexInputBindingDescription> bindingDescriptions = {
            {0, sizeof(Vertex), vk::VertexInputRate::eVertex}, 
            {1, sizeof(ObjectInstance), vk::VertexInputRate::eInstance}
        };

        for (uint32_t i = 0; i < vertexAttributeDescription.size(); ++i)
            vertexAttributeDescription[i].binding = 0;

        for (uint32_t i = 0; i < instanceAttributeDescription.size(); ++i)
            instanceAttributeDescription[i].binding = 1;
        
        // merge all vertex input data
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
        attributeDescriptions.insert(attributeDescriptions.end(), vertexAttributeDescription.begin(), vertexAttributeDescription.end());
        attributeDescriptions.insert(attributeDescriptions.end(), instanceAttributeDescription.begin(), instanceAttributeDescription.end());
        
        for (uint32_t i = 0; i < attributeDescriptions.size(); ++i)
            attributeDescriptions[i].location = i;

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
        if (m_blitPass) {
            vertexInputInfo.vertexBindingDescriptionCount = 0;
            vertexInputInfo.pVertexBindingDescriptions = nullptr;
            vertexInputInfo.vertexAttributeDescriptionCount = 0;
            vertexInputInfo.pVertexAttributeDescriptions = nullptr;
        }
        else {
            vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
            vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
            vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
            vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
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
        // default setup for color attachments
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

        std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments(m_colorAttachmentCount, colorBlendAttachment);

        vk::PipelineColorBlendStateCreateInfo colorBlending { 
            .logicOpEnable = VK_FALSE,
            .logicOp = vk::LogicOp::eCopy,
            .attachmentCount = m_colorAttachmentCount,
            .pAttachments = colorBlendAttachments.data(),
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
            .layout = m_pipelineLayout,
            .renderPass = m_renderPass,
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

    void setRenderPass(vk::RenderPass renderPass) {
        m_renderPass = renderPass;
    }

    void setPipelineLayout(vk::PipelineLayout pipelineLayout) {
        m_pipelineLayout = pipelineLayout;
    }

    void setDescriptorSetLayout(vk::DescriptorSetLayout descriptorSetLayout) {
        m_descriptorSetLayout = descriptorSetLayout;
    }

    void setBlitPass(bool isBlitPass) {
        m_blitPass = isBlitPass;
    }

    void setColorAttachmentCount(uint32_t colorAttachmentCount) {
        m_colorAttachmentCount = colorAttachmentCount;
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
    vk::RenderPass m_renderPass {nullptr};
    vk::DescriptorSetLayout m_descriptorSetLayout {nullptr};
    vk::PipelineLayout m_pipelineLayout {nullptr};
    uint32_t m_colorAttachmentCount {0};

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

    RenderPass(VulkanContext* pContext, vk::CommandPool commandPool, std::shared_ptr<ResourceManager> pResourceManager, std::shared_ptr<Scene> pScene);
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
    vk::CommandPool m_commandPool {VK_NULL_HANDLE};

    vk::Extent2D m_extent;

    uint32_t m_colorAttachmentCount {0};

    std::shared_ptr<ResourceManager> m_pResourceManager {nullptr};
    std::shared_ptr<Scene> m_pScene {nullptr};

    void createDescriptorSet(const std::vector<ResourceBindingInfo>& bindingInfos);
    void createFramebuffer(const std::vector<AttachmentInfo>& colorAttachmentInfos, const AttachmentInfo& depthStencilAttachmentInfo);
    void createVkRenderPass(const std::vector<AttachmentInfo>& colorAttachmentInfos, const AttachmentInfo& depthStencilAttachmentInfo);
    void createRasterPipeline(const std::string& vertShaderPath, const std::string& fragShaderPath, bool isBlitPass = false);

}; // class RenderPass

} // namespace vrb

#endif // RENDER_PASS_HPP