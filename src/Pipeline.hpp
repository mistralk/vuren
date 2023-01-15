#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include "Utils.hpp"
#include "Common.hpp"
#include "ResourceManager.hpp"

namespace vuren {

struct RasterProperties {
    vk::RenderPass renderPass {VK_NULL_HANDLE};
    vk::Framebuffer framebuffer {VK_NULL_HANDLE};
    bool isBiltPass {false};
    uint32_t colorAttachmentCount {0};
    std::string vertShaderPath;
    std::string fragShaderPath;
};

struct RayTracingProperties {
    std::string raygenShaderPath;
    std::string missShaderPath;
    std::string closestHitShaderPath;
    std::vector<AccelerationStructure> blas;
    AccelerationStructure tlas {VK_NULL_HANDLE};
};

class Pipeline {
public:
    Pipeline(VulkanContext* pContext, vk::DescriptorSetLayout descriptorSetLayout)
     : m_pContext(pContext), m_descriptorSetLayout(descriptorSetLayout) {
    }

    virtual ~Pipeline() {}

    Pipeline() = delete;

    virtual void cleanup() {
        if (m_pipeline) m_pContext->m_device.destroyPipeline(m_pipeline, nullptr);
        if (m_pipelineLayout) m_pContext->m_device.destroyPipelineLayout(m_pipelineLayout, nullptr);
    }

    virtual void setup() = 0;

    vk::Pipeline getPipeline() {
        return m_pipeline;
    }

    vk::PipelineLayout getPipelineLayout() {
        return m_pipelineLayout;
    }

    void setDescriptorSetLayout(vk::DescriptorSetLayout descriptorSetLayout) {
        m_descriptorSetLayout = descriptorSetLayout;
    }

protected:
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

    vk::Pipeline m_pipeline {VK_NULL_HANDLE};
    vk::PipelineLayout m_pipelineLayout {VK_NULL_HANDLE};

    VulkanContext* m_pContext {nullptr};
    vk::DescriptorSetLayout m_descriptorSetLayout {VK_NULL_HANDLE};


}; // class Pipeline

class RasterizationPipeline : public Pipeline {
public:
    RasterizationPipeline(VulkanContext* pContext, vk::DescriptorSetLayout descriptorSetLayout, RasterProperties rasterProperties);

    void setup() override;

private:
    RasterProperties m_rasterProperties;

}; // class RasterizationPipeline


class RayTracingPipeline : public Pipeline {
public:
    RayTracingPipeline(VulkanContext* pContext, vk::DescriptorSetLayout descriptorSetLayout, RayTracingProperties rayTracingProperties);

    void setup() override;

private:
    RayTracingProperties m_rayTracingProperties;
    std::vector<vk::RayTracingShaderGroupCreateInfoKHR> m_shaderGroups;

}; // class RayTracingPipeline

} // namespace vuren


#endif // PIPELINE_HPP