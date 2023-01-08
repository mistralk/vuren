#ifndef RENDER_PASS_HPP
#define RENDER_PASS_HPP

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include "Utils.hpp"
#include "Common.hpp"
#include "Scene.hpp"
#include "ResourceManager.hpp"
#include "Pipeline.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace vrb {

class VulkanContext;

class RenderPass {
public:
    enum PipelineType {
        eRasterization,
        eRayTracing,
        // eCompute
    };

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

    RenderPass(PipelineType pipelineType)
        : m_pipelineType(pipelineType) {
    }

    virtual ~RenderPass() {}

    virtual void init(VulkanContext* pContext, vk::CommandPool commandPool, std::shared_ptr<ResourceManager> pResourceManager, std::shared_ptr<Scene> pScene) {
        m_pContext = pContext;
        m_commandPool = commandPool;
        m_pResourceManager = pResourceManager; 
        m_pScene = pScene;
    }
    
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
    PipelineType m_pipelineType;
    std::unique_ptr<Pipeline> m_pPipeline {nullptr};

    RasterProperties m_rasterProperties;
    RayTracingProperties m_rayTracingProperties;

    vk::RenderPass m_renderPass {VK_NULL_HANDLE};
    vk::Framebuffer m_framebuffer {VK_NULL_HANDLE};
    vk::DescriptorSetLayout m_descriptorSetLayout {VK_NULL_HANDLE};
    vk::PipelineLayout m_pipelineLayout {VK_NULL_HANDLE};
    vk::DescriptorPool m_descriptorPool {VK_NULL_HANDLE};
    vk::DescriptorSet m_descriptorSet {VK_NULL_HANDLE};

    VulkanContext* m_pContext {nullptr};
    vk::CommandPool m_commandPool {VK_NULL_HANDLE};

    vk::Extent2D m_extent;

    std::shared_ptr<ResourceManager> m_pResourceManager {nullptr};
    std::shared_ptr<Scene> m_pScene {nullptr};

    void createDescriptorSet(const std::vector<ResourceBindingInfo>& bindingInfos);
    void createFramebuffer(const std::vector<AttachmentInfo>& colorAttachmentInfos, const AttachmentInfo& depthStencilAttachmentInfo);
    void createVkRenderPass(const std::vector<AttachmentInfo>& colorAttachmentInfos, const AttachmentInfo& depthStencilAttachmentInfo);
    
    void setupRasterPipeline(const std::string& vertShaderPath, const std::string& fragShaderPath, bool isBlitPass = false);
    void setupRayTracingPipeline(const std::string& raygenShaderPath, const std::string& missShaderPath, const std::string& closestHitShaderPath, const std::vector<AccelerationStructure>& blas, AccelerationStructure tlas);

}; // class RenderPass

} // namespace vrb

#endif // RENDER_PASS_HPP