#ifndef RENDER_PASS_HPP
#define RENDER_PASS_HPP

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include "Common.hpp"
#include "ResourceManager.hpp"
#include "Scene.hpp"
#include "Utils.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace vuren {

class VulkanContext;

class RenderPass {
public:
    struct ResourceBindingInfo {
        std::string name;
        vk::DescriptorType descriptorType;
        vk::ShaderStageFlags stageFlags;
        uint32_t descriptorCount;
    };

    RenderPass();
    virtual ~RenderPass();

    virtual void init(VulkanContext *pContext, vk::CommandPool commandPool,
                      std::shared_ptr<ResourceManager> pResourceManager, std::shared_ptr<Scene> pScene);
    virtual void define()                                = 0;
    virtual void setup()                                 = 0;
    virtual void record(vk::CommandBuffer commandBuffer) = 0;
    virtual void cleanup();

    void createDescriptorSet(const std::vector<ResourceBindingInfo> &bindingInfos);
    vk::ShaderModule createShaderModule(const std::vector<char> &code);

    void setExtent(vk::Extent2D extent) { m_extent = extent; }

protected:
    vk::Pipeline m_pipeline{ VK_NULL_HANDLE };
    vk::PipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
    vk::DescriptorSetLayout m_descriptorSetLayout{ VK_NULL_HANDLE };
    vk::DescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
    vk::DescriptorSet m_descriptorSet{ VK_NULL_HANDLE };

    VulkanContext *m_pContext{ nullptr };
    vk::CommandPool m_commandPool{ VK_NULL_HANDLE };

    vk::Extent2D m_extent;

    AccelerationStructure m_tlas{ VK_NULL_HANDLE }; // this is only for ray tracing pass, but we need it in
                                                    // createDescriptorSet()

    std::shared_ptr<ResourceManager> m_pResourceManager{ nullptr };
    std::shared_ptr<Scene> m_pScene{ nullptr };

}; // class RenderPass

class RasterRenderPass : public RenderPass {
public:
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

    RasterRenderPass();
    ~RasterRenderPass();

    virtual void init(VulkanContext *pContext, vk::CommandPool commandPool,
                      std::shared_ptr<ResourceManager> pResourceManager, std::shared_ptr<Scene> pScene);
    virtual void define()                                = 0;
    virtual void record(vk::CommandBuffer commandBuffer) = 0;
    virtual void setup();
    virtual void cleanup();

    void setupRasterPipeline(const std::string &vertShaderPath, const std::string &fragShaderPath,
                             bool isBlitPass = false);
    void createFramebuffer(const std::vector<AttachmentInfo> &colorAttachmentInfos,
                           const AttachmentInfo &depthStencilAttachmentInfo);
    void createVkRenderPass(const std::vector<AttachmentInfo> &colorAttachmentInfos,
                            const AttachmentInfo &depthStencilAttachmentInfo);

    vk::RenderPass getRenderPass() { return m_renderPass; }

protected:
    vk::RenderPass m_renderPass{ VK_NULL_HANDLE };
    vk::Framebuffer m_framebuffer{ VK_NULL_HANDLE };
    bool m_isBiltPass{ false };
    uint32_t m_colorAttachmentCount{ 0 };
    std::string m_vertShaderPath;
    std::string m_fragShaderPath;

}; // class RasterRenderPass

class RayTracingRenderPass : public RenderPass {
public:
    struct BlasInput {
        std::vector<vk::AccelerationStructureGeometryKHR> asGeometry;
        std::vector<vk::AccelerationStructureBuildRangeInfoKHR> asBuildOffsetInfo;
        vk::BuildAccelerationStructureFlagsKHR flags{ 0 };
    };

    struct BuildAccelerationStructure {
        vk::AccelerationStructureBuildGeometryInfoKHR buildInfo;
        vk::AccelerationStructureBuildSizesInfoKHR sizeInfo;
        const vk::AccelerationStructureBuildRangeInfoKHR *rangeInfo;

        AccelerationStructure as;
        AccelerationStructure cleanupAs;
    };

    RayTracingRenderPass();
    ~RayTracingRenderPass();

    virtual void init(VulkanContext *pContext, vk::CommandPool commandPool,
                      std::shared_ptr<ResourceManager> pResourceManager, std::shared_ptr<Scene> pScene);
    virtual void define()                                = 0;
    virtual void record(vk::CommandBuffer commandBuffer) = 0;
    virtual void setup();
    virtual void cleanup();

    BlasInput objectToVkGeometryKHR(const SceneObject &object);
    void buildBlas(const std::vector<BlasInput> &input, vk::BuildAccelerationStructureFlagsKHR flags);
    void createBlas();
    void buildTlas(
        const std::vector<vk::AccelerationStructureInstanceKHR> &instances,
        vk::BuildAccelerationStructureFlagsKHR flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
        bool update                                  = false);
    void createTlas(const std::vector<ObjectInstance> &instances);
    uint32_t align_up(uint32_t size, uint32_t alignment);
    void createShaderBindingTable();

    void setupRayTracingPipeline(const std::string &raygenShaderPath, const std::string &missShaderPath,
                                 const std::string &closestHitShaderPath);

protected:
    std::string m_raygenShaderPath;
    std::string m_missShaderPath;
    std::string m_closestHitShaderPath;
    std::vector<AccelerationStructure> m_blas;

    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties;
    std::vector<vk::RayTracingShaderGroupCreateInfoKHR> m_shaderGroups;
    Buffer m_sbtBuffer;
    vk::StridedDeviceAddressRegionKHR m_rgenRegion{};
    vk::StridedDeviceAddressRegionKHR m_missRegion{};
    vk::StridedDeviceAddressRegionKHR m_hitRegion{};
    vk::StridedDeviceAddressRegionKHR m_callRegion{};

}; // class RayTracingRenderPass

} // namespace vuren

#endif // RENDER_PASS_HPP