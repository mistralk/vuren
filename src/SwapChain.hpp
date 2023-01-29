#ifndef SWAP_CHAIN_HPP
#define SWAP_CHAIN_HPP

#include <limits>

#include "Common.hpp"
#include "VulkanContext.hpp"
#include "RenderPass.hpp"

namespace vuren {

class SwapChain;

// the common "final" render pass, which renders fullscreen triangle onto screen.
// this render pass is directly linked to swap chain.
class FinalRenderPass : public RasterRenderPass {
public:
    FinalRenderPass() {}

    ~FinalRenderPass() {}

    void setSwapChain(std::shared_ptr<SwapChain> pSwapChain) { m_pSwapChain = pSwapChain; }

    void init(VulkanContext *pContext, vk::CommandPool commandPool, std::shared_ptr<ResourceManager> pResourceManager,
              std::shared_ptr<Scene> pScene) override {
        RasterRenderPass::init(pContext, commandPool, pResourceManager, pScene);
    }

    void define() override;

    void record(vk::CommandBuffer commandBuffer) override;

    void createSwapChainFrameBuffers(Texture swapChainDepthImage);

    // required when changing resolution
    void updateDescriptorSets();

private:
    std::shared_ptr<SwapChain> m_pSwapChain{ nullptr };

}; // class FinalRenderPass

class SwapChain {
public:
    SwapChain(VulkanContext *pContext, GLFWwindow *pWindow) : m_pContext(pContext), m_pWindow(pWindow) {}
    ~SwapChain() {}

    SwapChain() = delete;

    void cleanupSwapChain();

    void createSwapChain();
    void createSwapChainImageViews();
    void createFrameBuffers(vk::RenderPass renderPass, Texture swapChainDepthImage);

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats);
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities);
    
    void recreateSwapChain();

    vk::Extent2D getExtent() { return m_extent; }

    vk::SwapchainKHR getVkSwapChain() { return m_swapChain; }

    uint32_t getImageCount() { return m_imageCount; }

    std::shared_ptr<std::vector<vk::Framebuffer>> getSwapChainFrameBuffers() { return m_swapChainFramebuffers; }

    std::shared_ptr<std::vector<vk::Image>> getSwapChainColorImages() { return m_swapChainColorImages; }

    std::shared_ptr<std::vector<vk::ImageView>> getSwapChainColorImageViews() { return m_swapChainColorImageViews; }

    void updateImageIndex(uint32_t newIndex) { m_imageIndex = newIndex; }

    uint32_t getImageIndex() { return m_imageIndex; }

private:
    GLFWwindow *m_pWindow{ nullptr };
    VulkanContext *m_pContext{ nullptr };

    vk::SwapchainKHR m_swapChain{ VK_NULL_HANDLE };
    vk::Format m_imageFormat;
    vk::Extent2D m_extent;
    uint32_t m_imageCount{ 0 };
    uint32_t m_imageIndex{ 0 };

    std::shared_ptr<std::vector<vk::Framebuffer>> m_swapChainFramebuffers;
    std::shared_ptr<std::vector<vk::Image>> m_swapChainColorImages;
    std::shared_ptr<std::vector<vk::ImageView>> m_swapChainColorImageViews;

}; // class SwapChain

} // namespace vuren

#endif // SWAP_CHAIN_HPP