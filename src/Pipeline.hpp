#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include "Utils.hpp"
#include "Common.hpp"
#include "ResourceManager.hpp"

namespace vrb {

struct RasterProperties {
    bool isBiltPass {false};
    uint32_t colorAttachmentCount;
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
    Pipeline(VulkanContext* pContext, vk::RenderPass renderPass, vk::DescriptorSetLayout descriptorSetLayout, vk::PipelineLayout pipelineLayout)
     : m_pContext(pContext), m_renderPass(renderPass), m_descriptorSetLayout(descriptorSetLayout), m_pipelineLayout(pipelineLayout) {
    }

    virtual ~Pipeline() {}

    Pipeline() {}

    virtual void cleanup() {
        m_pContext->m_device.destroyPipeline(m_pipeline, nullptr);
    }

    virtual void setup() = 0;

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

    vk::Pipeline m_pipeline;

    VulkanContext* m_pContext {nullptr};
    vk::RenderPass m_renderPass {nullptr};
    vk::DescriptorSetLayout m_descriptorSetLayout {nullptr};
    vk::PipelineLayout m_pipelineLayout {nullptr};


}; // class Pipeline

// class RayTracingPipeline : public Pipeline {
// public:
//     RayTracingPipeline(VulkanContext* pContext, vk::RenderPass renderPass, vk::DescriptorSetLayout descriptorSetLayout, vk::PipelineLayout pipelineLayout, RayTracingProperties rayTracingProperties)
//      : Pipeline(pContext, renderPass, descriptorSetLayout, pipelineLayout), m_rayTracingProperties(rayTracingProperties) {
//     }

//     ~RayTracingPipeline() {
//     }

//     void setup() {
//         enum StageIndices {
//             eRaygen,
//             eMiss,
//             eClosestHit,
//             eShaderGroupCount
//         };

//         const std::string raygenShaderPath = "shaders/rt.rgen.spv";
//         const std::string missShaderPath = "shaders/miss.rgen.spv";
//         const std::string closestHitShaderPath = "shaders/closesthit.rgen.spv";

//         auto raygenShaderCode = readFile(raygenShaderPath);
//         auto missShaderCode = readFile(missShaderPath);
//         auto closestHitShaderCode = readFile(closestHitShaderPath);

//         std::array<vk::PipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
//         vk::PipelineShaderStageCreateInfo stage { .pName = "main" };

//         // raygen
//         stage.module = createShaderModule(raygenShaderCode);
//         stage.stage = vk::ShaderStageFlagBits::eRaygenKHR;
//         stages[eRaygen] = stage;

//         // miss
//         stage.module = createShaderModule(missShaderCode);
//         stage.stage = vk::ShaderStageFlagBits::eMissKHR;
//         stages[eMiss] = stage;

//         // hit group - closest hit
//         stage.module = createShaderModule(closestHitShaderCode);
//         stage.stage = vk::ShaderStageFlagBits::eClosestHitKHR;
//         stages[eClosestHit] = stage;

//         // shader groups
//         vk::RayTracingShaderGroupCreateInfoKHR group {
//             .generalShader = VK_SHADER_UNUSED_KHR,
//             .closestHitShader = VK_SHADER_UNUSED_KHR,
//             .anyHitShader = VK_SHADER_UNUSED_KHR,
//             .intersectionShader = VK_SHADER_UNUSED_KHR
//         };

//         // raygen
//         group.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
//         group.generalShader = eRaygen;
//         m_shaderGroups.push_back(group);

//         // miss
//         group.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
//         group.generalShader = eMiss;
//         m_shaderGroups.push_back(group);

//         // closest hit
//         group.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
//         group.generalShader = VK_SHADER_UNUSED_KHR;
//         group.closestHitShader = eClosestHit;
//         m_shaderGroups.push_back(group);

//         // setup the pipeline rayout that will describe how the pipeline will access external data
//         vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;

//         // push constant
//         vk::PushConstantRange pushConstant{vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR,
//                                            0, sizeof(PushConstantRay)};

//         vk::PipelineLayoutCreateInfo layoutCreateInfo {
//             .pushConstantRangeCount = 1,
//             .pPushConstantRanges = &pushConstant
//         };

//         // descriptor sets (TLAS and output)

//         // create pipeline layout

//         // ray tracing pipeline can contain an arbitrary number of stages
//         // depending on the number of active shaders in the scene.
        
//         // assemble the shader stages and recursion depth info
//         vk::RayTracingPipelineCreateInfoKHR rtPipelineInfo {
//             .stageCount = static_cast<uint32_t>(stages.size()),
//             .pStages = stages.data(),
//             .groupCount = static_cast<uint32_t>(m_shaderGroups.size()),
//             .pGroups = m_shaderGroups.data(),
//             .maxPipelineRayRecursionDepth = 1,
//             .layout = m_rtPipelineLayout
//         };

//         m_pContext->m_device.createRayTracingPipelinesKHR({}, {}, 1, &rtPipelineInfo, nullptr, &m_pipeline);

//         for (auto& stage : stages) {
//             m_pContext->m_device.destroyShaderModule(stage.module, nullptr);
//         }
//     }

// private:
//     RayTracingProperties m_rayTracingProperties;
//     std::vector<vk::RayTracingShaderGroupCreateInfoKHR> m_shaderGroups;

// }; // class RayTracingPipeline

class RasterizationPipeline : public Pipeline {
public:
    RasterizationPipeline(VulkanContext* pContext, vk::RenderPass renderPass, vk::DescriptorSetLayout descriptorSetLayout, vk::PipelineLayout pipelineLayout, RasterProperties rasterProperties)
     : Pipeline(pContext, renderPass, descriptorSetLayout, pipelineLayout), m_rasterProperties(rasterProperties) {
    }

    ~RasterizationPipeline() {
    }

    void setup();

private:
    RasterProperties m_rasterProperties;

}; // class RasterizationPipeline

} // namespace vrb


#endif // PIPELINE_HPP