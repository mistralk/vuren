#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <set>
#include <cstring>
#include <optional>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <fstream>
#include <array>
#include <chrono>
#include <unordered_map>
#include <memory>
#include <random>

#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
    VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

#include "VulkanContext.hpp"
#include "Common.hpp"
#include "RenderPass.hpp"
#include "ResourceManager.hpp"
#include "Utils.hpp"
#include "Timer.hpp"
#include "Scene.hpp"

namespace vrb {

// temporary: for output texture control in GUI
std::vector<std::string> kOffscreenOutputTextureNames;
int kCurrentItem = 0;
bool kDirty = false;

const uint32_t kInstanceCount = 10;

class OffscreenRenderPass : public RenderPass {
public:
    OffscreenRenderPass(VulkanContext* pContext, vk::CommandPool commandPool, std::shared_ptr<ResourceManager> pResourceManager, std::shared_ptr<Scene> pScene) 
        : RenderPass(pContext, commandPool, pResourceManager, pScene) {
    }

    ~OffscreenRenderPass() {

    }

    OffscreenRenderPass() {}

    void setup() {
        // create textures for the attachments
        m_pResourceManager->createTextureRGBA32Sfloat("RasterColor");
        m_pResourceManager->createTextureRGBA32Sfloat("RasterPosWorld");
        m_pResourceManager->createTextureRGBA32Sfloat("RasterNormalWorld");
        m_pResourceManager->createDepthTexture("RasterDepth");

        kOffscreenOutputTextureNames.push_back("RasterColor");
        kOffscreenOutputTextureNames.push_back("RasterPosWorld");
        kOffscreenOutputTextureNames.push_back("RasterNormalWorld");

        // create a descriptor set
        std::vector<ResourceBindingInfo> bindings = {
            {"CameraBuffer", vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex},
            {"ModelTexture", vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment}
        };
        createDescriptorSet(bindings);

        // create framebuffers for the attachments
        std::vector<AttachmentInfo> colorAttachments = {
            {
                .imageView = m_pResourceManager->getTexture("RasterColor").descriptorInfo.imageView, 
                .format = vk::Format::eR32G32B32A32Sfloat, 
                .oldLayout = vk::ImageLayout::eUndefined, 
                .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                .srcAccessMask = vk::AccessFlagBits::eNone,
                .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite
            },
            {
                .imageView = m_pResourceManager->getTexture("RasterPosWorld").descriptorInfo.imageView, 
                .format = vk::Format::eR32G32B32A32Sfloat, 
                .oldLayout = vk::ImageLayout::eUndefined, 
                .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                .srcAccessMask = vk::AccessFlagBits::eNone,
                .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite
            },
            {
                .imageView = m_pResourceManager->getTexture("RasterNormalWorld").descriptorInfo.imageView, 
                .format = vk::Format::eR32G32B32A32Sfloat, 
                .oldLayout = vk::ImageLayout::eUndefined, 
                .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                .srcAccessMask = vk::AccessFlagBits::eNone,
                .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite
            }
        };

        AttachmentInfo depthStencilAttachment = {
            .imageView = m_pResourceManager->getTexture("RasterDepth").descriptorInfo.imageView,
            .format = findDepthFormat(*m_pContext), 
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .srcStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests,
            .dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests,
            .srcAccessMask = vk::AccessFlagBits::eNone,
            .dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite
        };

        // create a vulkan render pass object
        createVkRenderPass(colorAttachments, depthStencilAttachment);

        createFramebuffer(colorAttachments, depthStencilAttachment);

        // create a graphics pipeline for this render pass
        createRasterPipeline("shaders/shader.vert.spv", "shaders/shader.frag.spv");
    }

    void record(vk::CommandBuffer commandBuffer) {
        std::array<vk::ClearValue, 4> clearValues {};
        clearValues[0].color = vk::ClearColorValue {std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].color = vk::ClearColorValue {std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[2].color = vk::ClearColorValue {std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[3].depthStencil = vk::ClearDepthStencilValue {1.0f, 0};

        vk::RenderPassBeginInfo renderPassInfo { 
            .renderPass = m_renderPass,
            .framebuffer = m_framebuffer,
            .renderArea {
                .offset = {0, 0},
                .extent = m_extent },
            .clearValueCount = static_cast<uint32_t>(clearValues.size()),
            .pClearValues = clearValues.data()
        };
        
        commandBuffer.beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_rasterPipeline.getPipeline());

        vk::Viewport viewport { 
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(m_extent.width),
            .height = static_cast<float>(m_extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        commandBuffer.setViewport(0, 1, &viewport);

        vk::Rect2D scissor { 
            .offset = {0, 0},
            .extent = m_extent 
        };
        commandBuffer.setScissor(0, 1, &scissor);

        uint32_t objSize = m_pScene->getObjects().size();
        std::vector<vk::Buffer> vertexBuffers(objSize);
        vk::DeviceSize offsets[] = {0};

        for (uint32_t i = 0; i < objSize; ++i) {
            auto object = m_pScene->getObject(i);
            vertexBuffers[i] = object.vertexBuffer->descriptorInfo.buffer;

            vk::Buffer instanceBuffer = m_pResourceManager->getBuffer("InstanceBuffer").descriptorInfo.buffer;

            commandBuffer.bindVertexBuffers(0, 1, vertexBuffers.data(), offsets);
            commandBuffer.bindVertexBuffers(1, 1, &instanceBuffer, offsets);
            commandBuffer.bindIndexBuffer(object.indexBuffer->descriptorInfo.buffer, 0, vk::IndexType::eUint32);
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
            commandBuffer.drawIndexed(object.indexBufferSize, m_pScene->getInstanceCount(), 0, 0, 0);
        }

        commandBuffer.endRenderPass();
    }

private:
    vk::Format m_colorAttachmentFormat {vk::Format::eR32G32B32A32Sfloat};

}; // class OffscreenRenderPass

class FinalRenderPass : public RenderPass {
public:
    FinalRenderPass(VulkanContext* pContext, vk::CommandPool commandPool, std::shared_ptr<ResourceManager> pResourceManager, std::shared_ptr<Scene> pScene) 
        : RenderPass(pContext, commandPool, pResourceManager, pScene) {
    }

    ~FinalRenderPass() {

    }

    FinalRenderPass(){}

    void setSwapChainImagePointers(std::shared_ptr<std::vector<vk::Framebuffer>> swapChainFramebuffers, std::shared_ptr<std::vector<vk::Image>> swapChainColorImages, std::shared_ptr<std::vector<vk::ImageView>> swapChainColorImageViews) {
        m_swapChainFramebuffers = swapChainFramebuffers;
        m_swapChainColorImages = swapChainColorImages;
        m_swapChainColorImageViews = swapChainColorImageViews;
    }

    void setup() {
        // create the attachment images
        // global dict is not required for swap chain images
        m_pResourceManager->createDepthTexture("FinalDepth");

        // create a descriptor set
        std::vector<ResourceBindingInfo> bindings = {
            {kOffscreenOutputTextureNames[kCurrentItem], vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment}
        };
        createDescriptorSet(bindings);

        // create the renderpass
        std::vector<AttachmentInfo> colorAttachments; 
        AttachmentInfo colorAttachment { 
            .imageView = VK_NULL_HANDLE, 
            .format = vk::Format::eB8G8R8A8Srgb, 
            .oldLayout = vk::ImageLayout::eUndefined, 
            .newLayout = vk::ImageLayout::ePresentSrcKHR,
            .srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe,
            .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
            .srcAccessMask = vk::AccessFlagBits::eMemoryRead,
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead
        };
        colorAttachments.push_back(colorAttachment);
        AttachmentInfo depthStencilAttachment = {
            .imageView = m_pResourceManager->getTexture("FinalDepth").descriptorInfo.imageView,
            .format = findDepthFormat(*m_pContext), 
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .srcStageMask = vk::PipelineStageFlagBits::eNone,
            .dstStageMask = vk::PipelineStageFlagBits::eNone,
            .srcAccessMask = vk::AccessFlagBits::eNone,
            .dstAccessMask = vk::AccessFlagBits::eNone
        };
        createVkRenderPass(colorAttachments, depthStencilAttachment);

        // create swap chain framebuffers
        createSwapChainFrameBuffers(m_pResourceManager->getTexture("FinalDepth"));

        // create a graphics pipeline for this render pass
        createRasterPipeline("shaders/final.vert.spv", "shaders/final.frag.spv", true);
    }

    void record(vk::CommandBuffer commandBuffer) {
        std::array<vk::ClearValue, 2> clearValues {};
        clearValues[0].color = vk::ClearColorValue {std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = vk::ClearDepthStencilValue {1.0f, 0};

        vk::RenderPassBeginInfo renderPassInfo { 
            .renderPass = m_renderPass,
            .framebuffer = (*m_swapChainFramebuffers)[m_swapChainImageIndex],
            .renderArea { 
                .offset = {0, 0},
                .extent = m_extent },
            .clearValueCount = static_cast<uint32_t>(clearValues.size()),
            .pClearValues = clearValues.data()
        };
        
        commandBuffer.beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_rasterPipeline.getPipeline());

        vk::Viewport viewport { 
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(m_extent.width),
            .height = static_cast<float>(m_extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        commandBuffer.setViewport(0, 1, &viewport);

        vk::Rect2D scissor { 
            .offset = {0, 0},
            .extent = m_extent 
        };
        commandBuffer.setScissor(0, 1, &scissor);

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
        
        commandBuffer.draw(3, 1, 0, 0);

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

        commandBuffer.endRenderPass();
    }

    void updateSwapChainImageIndex(uint32_t newIndex) {
        m_swapChainImageIndex = newIndex;
    }

    void createSwapChainFrameBuffers(Texture swapChainDepthImage) {
        m_swapChainFramebuffers->resize(m_swapChainColorImageViews->size());

        for (size_t i = 0; i < m_swapChainFramebuffers->size(); ++i) {
            // Color attachment differs for every swap chain image,
            // but the same depth image can be used by all of them
            // because only a single subpass is running at the same time due to out semaphores
            std::array<vk::ImageView, 2> attachments = {
                (*m_swapChainColorImageViews)[i],
                swapChainDepthImage.descriptorInfo.imageView
            };

            vk::FramebufferCreateInfo framebufferInfo { 
                .renderPass = m_renderPass,
                .attachmentCount = static_cast<uint32_t>(attachments.size()),
                .pAttachments = attachments.data(),
                .width = m_extent.width,
                .height = m_extent.height,
                .layers = 1
            };

            if (m_pContext->m_device.createFramebuffer(&framebufferInfo, nullptr, &(*m_swapChainFramebuffers)[i]) != vk::Result::eSuccess) {
                throw std::runtime_error("failed to create swapchain framebuffer!");
            }
        }

        m_colorAttachmentCount = 1;
    }

    void updateDescriptorSets() {
        std::vector<ResourceBindingInfo> bindings = {
            {kOffscreenOutputTextureNames[kCurrentItem], vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment}
        };

        std::vector<vk::WriteDescriptorSet> descriptorWrites;

        for (size_t i = 0; i < bindings.size(); ++i) {
            vk::DescriptorImageInfo imageInfo;
            vk::DescriptorBufferInfo bufferInfo;

            vk::DescriptorBufferInfo* pBufferInfo = nullptr;
            vk::DescriptorImageInfo* pImageInfo = nullptr;

            switch (bindings[i].descriptorType) {
                case vk::DescriptorType::eCombinedImageSampler:
                    imageInfo.sampler = m_pResourceManager->getTexture(bindings[i].name).descriptorInfo.sampler;
                    imageInfo.imageView = m_pResourceManager->getTexture(bindings[i].name).descriptorInfo.imageView;
                    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                    pImageInfo = &imageInfo;
                    break;
                
                case vk::DescriptorType::eUniformBuffer:
                    bufferInfo.buffer = m_pResourceManager->getBuffer(bindings[i].name).descriptorInfo.buffer;
                    bufferInfo.offset = 0;
                    bufferInfo.range = sizeof(Camera);
                    pBufferInfo = &bufferInfo;
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

private:
    uint32_t m_swapChainImageIndex;

    std::shared_ptr<std::vector<vk::Framebuffer>> m_swapChainFramebuffers;
    std::shared_ptr<std::vector<vk::Image>> m_swapChainColorImages;
    std::shared_ptr<std::vector<vk::ImageView>> m_swapChainColorImageViews;

}; // class FinalRenderPass

class Application {
private:
    GLFWwindow* m_pWindow;

    VulkanContext m_vkContext;

    // swap chain
    vk::SwapchainKHR m_swapChain;
    vk::Format m_swapChainImageFormat;
    vk::Extent2D m_swapChainExtent;
    uint32_t m_imageCount;
    std::shared_ptr<std::vector<vk::Framebuffer>> m_swapChainFramebuffers;
    std::shared_ptr<std::vector<vk::Image>> m_swapChainColorImages;
    std::shared_ptr<std::vector<vk::ImageView>> m_swapChainColorImageViews;

    vk::CommandPool m_commandPool;
    std::vector<vk::CommandBuffer> m_commandBuffers;

    vk::Semaphore m_imageAvailableSemaphore;
    vk::Semaphore m_renderFinishedSemaphore;
    vk::Fence m_inFlightFence;

    bool m_framebufferResized = false;

    std::shared_ptr<ResourceManager> m_pResourceManager {nullptr};

    // scene description
    std::shared_ptr<Scene> m_pScene;
    std::vector<ObjectInstance> m_instances;

    // for the offscreen pass
    OffscreenRenderPass offscreenRenderPass;

    // for the final pass and gui
    // this pass is directly presented into swap chain framebuffers
    FinalRenderPass finalRenderPass;
    vk::DescriptorPool m_imguiDescriptorPool; // additional descriptor pool for imgui

    // ray tracing
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties;

public:
    Application() {

    }

    void run() {
        initWindow();
        initVulkan();
        initRayTracing();
        createBlas();
        createTlas();
        initImGui();
        mainLoop();
        cleanup();
    }

private:
    void initImGui() {
        // create descriptor pool for imgui
        vk::DescriptorPoolSize poolSizes[] = {
            { vk::DescriptorType::eSampler, 1000 },
            { vk::DescriptorType::eCombinedImageSampler, 1000 },
            { vk::DescriptorType::eSampledImage, 1000 },
            { vk::DescriptorType::eStorageImage, 1000 },
            { vk::DescriptorType::eUniformTexelBuffer, 1000 },
            { vk::DescriptorType::eStorageTexelBuffer, 1000 },
            { vk::DescriptorType::eUniformBuffer, 1000 },
            { vk::DescriptorType::eStorageBuffer, 1000 },
            { vk::DescriptorType::eUniformBufferDynamic, 1000 },
            { vk::DescriptorType::eStorageBufferDynamic, 1000 },
            { vk::DescriptorType::eInputAttachment, 1000 }
        };
        
        vk::DescriptorPoolCreateInfo poolCreateInfo = { 
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = 1000,
            .poolSizeCount = std::size(poolSizes),
            .pPoolSizes = poolSizes
        };

        if (m_vkContext.m_device.createDescriptorPool(&poolCreateInfo, nullptr, &m_imguiDescriptorPool) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create descriptor pool for imgui!");
        }

        // initialize imgui
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForVulkan(m_pWindow, true);

        ImGui_ImplVulkan_InitInfo initInfo = { 
            .Instance = m_vkContext.m_instance,
            .PhysicalDevice = m_vkContext.m_physicalDevice,
            .Device = m_vkContext.m_device,
            .Queue = m_vkContext.m_graphicsQueue,
            .DescriptorPool = m_imguiDescriptorPool,
            .MinImageCount = m_imageCount,
            .ImageCount = m_imageCount,
            .MSAASamples = VK_SAMPLE_COUNT_1_BIT
        };
        
        ImGui_ImplVulkan_Init(&initInfo, finalRenderPass.getRenderPass());

        ImGui::StyleColorsClassic();

        // execute a GPU command to upload imgui font textures
        vk::CommandBuffer commandBuffer = beginSingleTimeCommands(m_vkContext, m_commandPool);
        ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
        endSingleTimeCommands(m_vkContext, m_commandPool, commandBuffer);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    void updateGUI(float deltaTime) {
        // ImGui_ImplVulkan_NewFrame(); // do i need it?
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        auto imguiIO = ImGui::GetIO();
        imguiIO.DeltaTime = deltaTime;

        ImGui::Begin("Render Configuration");
        ImGui::Text("Statistics");
        ImGui::Text(" %.2f FPS (%.2f ms)", imguiIO.Framerate, imguiIO.DeltaTime);

        if (ImGui::BeginCombo("Output", kOffscreenOutputTextureNames[kCurrentItem].c_str(), 0)) {
            for (int i = 0; i < kOffscreenOutputTextureNames.size(); ++i) {
                const bool isSelected = (kCurrentItem == i);
                if (ImGui::Selectable(kOffscreenOutputTextureNames[i].c_str(), isSelected)) {
                    kCurrentItem = i;
                    kDirty = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::End();
    }

    void initWindow() {
        glfwInit();

        // tell glfw to not create an OpenGL context
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        
        // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_pWindow = glfwCreateWindow(kWidth, kHeight, kAppName.c_str(), nullptr, nullptr);
        glfwSetWindowUserPointer(m_pWindow, this);
        glfwSetFramebufferSizeCallback(m_pWindow, framebufferResizeCallback);
    }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        app->m_framebufferResized = true;
    }

    void initRayTracing() {
        vk::PhysicalDeviceProperties2 prop2 {
            .pNext = &m_rtProperties
        };
        m_vkContext.m_physicalDevice.getProperties2(&prop2);
    }

    struct BlasInput {
        std::vector<vk::AccelerationStructureGeometryKHR> asGeometry;
        std::vector<vk::AccelerationStructureBuildRangeInfoKHR> asBuildOffsetInfo;
        vk::BuildAccelerationStructureFlagsKHR flags {0};
    };

    BlasInput objectToVkGeometryKHR(const SceneObject& object) {
        vk::BufferDeviceAddressInfo vertexBufferAddressInfo = { .buffer = object.vertexBuffer->descriptorInfo.buffer };
        vk::BufferDeviceAddressInfo indexBufferAddressInfo = { .buffer = object.indexBuffer->descriptorInfo.buffer };
        
        // only the position attribute is needed for the AS build.
        // if position is not the first member of Vertex,
        // we have to manually adjust vertexAddress using offsetof.
        vk::DeviceAddress vertexAddress = m_vkContext.m_device.getBufferAddress(vertexBufferAddressInfo);
        vk::DeviceAddress indexAddress = m_vkContext.m_device.getBufferAddress(indexBufferAddressInfo);

        uint32_t maxPrimitiveCount = object.indexBufferSize / 3;

        vk::AccelerationStructureGeometryTrianglesDataKHR triangles {
            // describe buffer as array of vertexobj
            .vertexFormat = vk::Format::eR32G32B32Sfloat,
            .vertexData = {.deviceAddress = vertexAddress},
            .vertexStride = sizeof(Vertex),
            .maxVertex = object.vertexBufferSize,
            // describe index data
            .indexType = vk::IndexType::eUint32,
            .indexData = {.deviceAddress = indexAddress},
            .transformData = {} // identity transform
        };

        vk::AccelerationStructureGeometryKHR asGeom {
            .geometryType = vk::GeometryTypeKHR::eTriangles,
            .geometry = {.triangles = triangles},
            .flags = vk::GeometryFlagBitsKHR::eOpaque
        };

        vk::AccelerationStructureBuildRangeInfoKHR offset {
            .primitiveCount = maxPrimitiveCount,
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        };

        BlasInput input;
        input.asGeometry.emplace_back(asGeom);
        input.asBuildOffsetInfo.emplace_back(offset);

        return input;
    }

    struct BuildAccelerationStructure {
        vk::AccelerationStructureBuildGeometryInfoKHR buildInfo;
        vk::AccelerationStructureBuildSizesInfoKHR sizeInfo;
        const vk::AccelerationStructureBuildRangeInfoKHR* rangeInfo;

        AccelerationStructure as;
        AccelerationStructure cleanupAs;
    };
    std::vector<AccelerationStructure> m_blas;
    AccelerationStructure m_tlas;

    // generate one BLAS for each BlasInput
    void buildBlas(const std::vector<BlasInput>& input, vk::BuildAccelerationStructureFlagsKHR flags) {

        uint32_t blasCount = static_cast<uint32_t>(input.size());
        vk::DeviceSize asTotalSize = 0; // all aloocated BLAS
        uint32_t compactionsSize = 0; // BLAS requesting compaction
        vk::DeviceSize maxScratchSize = 0; 

        std::vector<BuildAccelerationStructure> buildAs(blasCount);

        // prepare the information for the acceleration build commands
        for (uint32_t i = 0; i < blasCount; ++i) {
            buildAs[i].buildInfo = {
                .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
                .flags = input[i].flags | flags,
                .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
                .geometryCount = static_cast<uint32_t>(input[i].asGeometry.size()),
                .pGeometries = input[i].asGeometry.data()
            };

            buildAs[i].rangeInfo = input[i].asBuildOffsetInfo.data();

            // find sizes to create acceleration structure and scratch
            std::vector<uint32_t> maxPrimCount(input[i].asBuildOffsetInfo.size());
            for (auto tt = 0; tt < input[i].asBuildOffsetInfo.size(); ++tt)
                maxPrimCount[tt] = input[i].asBuildOffsetInfo[tt].primitiveCount;
            
            m_vkContext.m_device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, &buildAs[i].buildInfo, maxPrimCount.data(), &buildAs[i].sizeInfo);

            asTotalSize += buildAs[i].sizeInfo.accelerationStructureSize;
            maxScratchSize = std::max(maxScratchSize, buildAs[i].sizeInfo.buildScratchSize);
            if ((buildAs[i].buildInfo.flags & vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction) == vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction)
                compactionsSize += 1;
        }

        // allocate the "largest" scratch buffer holding the temporary data of the acceleration structure builder
        Buffer scratchBuffer = createBuffer(m_vkContext, maxScratchSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);
        vk::BufferDeviceAddressInfo scratchBufferAddressInfo = { .buffer = scratchBuffer.descriptorInfo.buffer };
        vk::DeviceAddress scratchAddress = m_vkContext.m_device.getBufferAddress(scratchBufferAddressInfo);

        vk::QueryPool queryPool{VK_NULL_HANDLE};

        if (compactionsSize > 0) {
            assert(compactionsSize == blasCount);
            vk::QueryPoolCreateInfo poolCreateInfo = {
                .queryType = vk::QueryType::eAccelerationStructureCompactedSizeKHR,
                .queryCount = blasCount
            };
            if (m_vkContext.m_device.createQueryPool(&poolCreateInfo, nullptr, &queryPool) != vk::Result::eSuccess) {
                throw std::runtime_error("failed to create a query pool!");
            }
        }

        // batching creation/compaction of BLAS to allow staying in restricted amount of memory
        std::vector<uint32_t> indices;
        vk::DeviceSize batchSize = 0;
        vk::DeviceSize batchLimit = 256'000'000;

        for (uint32_t i = 0; i < blasCount; ++i) {
            indices.push_back(i);
            batchSize += buildAs[i].sizeInfo.accelerationStructureSize;

            // over the limit or last BLAS element
            if (batchSize >= batchLimit || i == blasCount - 1) {
                // create a command buffer
                vk::CommandBuffer commandBuffer = beginSingleTimeCommands(m_vkContext, m_commandPool);

                // create BLAS
                if (queryPool)
                    m_vkContext.m_device.resetQueryPool(queryPool, 0, static_cast<uint32_t>(indices.size()));
                uint32_t queryCount = 0;

                for (const auto& j : indices) {
                    vk::AccelerationStructureCreateInfoKHR createInfo {
                        .size = buildAs[j].sizeInfo.accelerationStructureSize,
                        .type = vk::AccelerationStructureTypeKHR::eBottomLevel
                    };

                    AccelerationStructure as;
                    createAs(m_vkContext, createInfo, as);
                    buildAs[j].as = as;
                    buildAs[j].buildInfo.dstAccelerationStructure = buildAs[j].as.as;
                    buildAs[j].buildInfo.scratchData.deviceAddress = scratchAddress;

                    commandBuffer.buildAccelerationStructuresKHR(1, &buildAs[j].buildInfo, &buildAs[j].rangeInfo);

                    vk::MemoryBarrier barrier {
                        .srcAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR,
                        .dstAccessMask = vk::AccessFlagBits::eAccelerationStructureReadKHR
                    };

                    commandBuffer.pipelineBarrier(
                        vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, // src
                        vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, // dst
                        {}, 1, &barrier, 0, nullptr, 0, nullptr);

                    if (queryPool) {
                        // add a query to find real amount of memory needed, use for compaction
                        commandBuffer.writeAccelerationStructuresPropertiesKHR(
                            1, &buildAs[j].buildInfo.dstAccelerationStructure,
                            vk::QueryType::eAccelerationStructureCompactedSizeKHR, queryPool, queryCount++);
                    }
                }

                // submit and wait
                endSingleTimeCommands(m_vkContext, m_commandPool, commandBuffer);

                if (queryPool) {
                    vk::CommandBuffer commandBuffer = beginSingleTimeCommands(m_vkContext, m_commandPool);

                    // compact BLAS

                    uint32_t queryCount = 0;
                    std::vector<AccelerationStructure> cleanupAs; // previous AS to destroy

                    // get the compacted size result back
                    std::vector<vk::DeviceSize> compactSizes(static_cast<uint32_t>(indices.size()));
                    if (m_vkContext.m_device.getQueryPoolResults(queryPool, 0, (uint32_t)compactSizes.size(), compactSizes.size() * sizeof(vk::DeviceSize), compactSizes.data(), sizeof(vk::DeviceSize), vk::QueryResultFlagBits::eWait) != vk::Result::eSuccess) {
                        throw std::runtime_error("failed to get query pool results!");
                    }

                    for (auto idx : indices) {
                        buildAs[idx].cleanupAs = buildAs[idx].as;
                        buildAs[idx].sizeInfo.accelerationStructureSize = compactSizes[queryCount++];

                        // create a compact version of the AS
                        vk::AccelerationStructureCreateInfoKHR asCreateInfo {
                            .size = buildAs[idx].sizeInfo.accelerationStructureSize,
                            .type = vk::AccelerationStructureTypeKHR::eBottomLevel
                        };
                        createAs(m_vkContext, asCreateInfo, buildAs[idx].as);

                        // copy the original BLAS to a compact version
                        vk::CopyAccelerationStructureInfoKHR copyInfo {
                          .src = buildAs[idx].buildInfo.dstAccelerationStructure,
                          .dst = buildAs[idx].as.as,
                          .mode = vk::CopyAccelerationStructureModeKHR::eCompact  
                        };
                        commandBuffer.copyAccelerationStructureKHR(&copyInfo);
                    }
                    
                    // submit and wait
                    endSingleTimeCommands(m_vkContext, m_commandPool, commandBuffer);

                    // destroyNonCompacted
                    for (auto& i : indices) {
                        destroyAs(m_vkContext, buildAs[i].cleanupAs);
                    }
                }

                batchSize = 0;
                indices.clear();
            }
        }

        for (auto& b : buildAs) {
            m_blas.emplace_back(b.as);
        }

        m_vkContext.m_device.destroyQueryPool(queryPool, nullptr);
        destroyBuffer(m_vkContext, scratchBuffer);
    }

    void createBlas() {
        // BLAS stores each primitive in a geometry.
        std::vector<BlasInput> allBlas;
        allBlas.reserve(m_pScene->getObjects().size());

        for (const auto& obj : m_pScene->getObjects()) {
            auto blas = objectToVkGeometryKHR(obj);
            allBlas.emplace_back(blas);
        }

        buildBlas(allBlas, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
    }

    void buildTlas(const std::vector<vk::AccelerationStructureInstanceKHR>& instances, vk::BuildAccelerationStructureFlagsKHR flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace, bool update = false) {
        assert(m_tlas.as == vk::AccelerationStructureKHR{VK_NULL_HANDLE} || update);
        uint32_t instanceCount = static_cast<uint32_t>(instances.size());

        vk::CommandBuffer commandBuffer = beginSingleTimeCommands(m_vkContext, m_commandPool);;

        // caution: this is not the managed "InstanceBuffer"
        Buffer instanceBuffer = createBuffer(m_vkContext, instanceCount * sizeof(vk::AccelerationStructureInstanceKHR), vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR, vk::MemoryPropertyFlagBits::eDeviceLocal);
        vk::BufferDeviceAddressInfo addressInfo = { .buffer = instanceBuffer.descriptorInfo.buffer };
        vk::DeviceAddress instanceBufferAddress = m_vkContext.m_device.getBufferAddress(&addressInfo);

        // make sure the copy of the instance buffer are copied before triggering the acceleration structure build
        vk::MemoryBarrier barrier {
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR
        };
        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
            {}, 1, &barrier, 0, nullptr, 0, nullptr);

        // create the TLAS
        vk::AccelerationStructureGeometryInstancesDataKHR instanceData;
        instanceData.data.deviceAddress = instanceBufferAddress;

        vk::AccelerationStructureGeometryKHR tlasGeometry;
        tlasGeometry.geometryType = vk::GeometryTypeKHR::eInstances;
        tlasGeometry.geometry.instances = instanceData;

        vk::AccelerationStructureBuildGeometryInfoKHR buildInfo {
            .type = vk::AccelerationStructureTypeKHR::eTopLevel,
            .flags = flags,
            .mode = update ? vk::BuildAccelerationStructureModeKHR::eUpdate : vk::BuildAccelerationStructureModeKHR::eBuild,
            .srcAccelerationStructure = VK_NULL_HANDLE,
            .geometryCount = 1,
            .pGeometries = &tlasGeometry
        };

        vk::AccelerationStructureBuildSizesInfoKHR sizeInfo{};
        m_vkContext.m_device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, &buildInfo, &instanceCount, &sizeInfo);

        vk::AccelerationStructureCreateInfoKHR createInfo {
            .size = sizeInfo.accelerationStructureSize,
            .type = vk::AccelerationStructureTypeKHR::eTopLevel
        };

        // create TLAS
        createAs(m_vkContext, createInfo, m_tlas);

        // allocate the scratch memory
        Buffer scratchBuffer = createBuffer(m_vkContext, sizeInfo.buildScratchSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);
        vk::BufferDeviceAddressInfo scratchBufferAddressInfo = { .buffer = scratchBuffer.descriptorInfo.buffer };
        vk::DeviceAddress scratchAddress = m_vkContext.m_device.getBufferAddress(scratchBufferAddressInfo);

        // update build information
        buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
        buildInfo.dstAccelerationStructure = m_tlas.as;
        buildInfo.scratchData.deviceAddress = scratchAddress;

        // build offsets info: n instances
        vk::AccelerationStructureBuildRangeInfoKHR offsetInfo {
            .primitiveCount = instanceCount,
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        };

        const vk::AccelerationStructureBuildRangeInfoKHR* pOffsetInfo = &offsetInfo;

        // build the TLAS
        commandBuffer.buildAccelerationStructuresKHR(1, &buildInfo, &pOffsetInfo);

        endSingleTimeCommands(m_vkContext, m_commandPool, commandBuffer);
        destroyBuffer(m_vkContext, scratchBuffer);
        destroyBuffer(m_vkContext, instanceBuffer);
    }

    void createTlas() {
        // TLAS is the entry point in the rt scene description
        std::vector<vk::AccelerationStructureInstanceKHR> tlas;
        tlas.reserve(m_instances.size());
        
        for (const ObjectInstance& instance : m_instances) {
            // glm: column-major matrix
            vk::TransformMatrixKHR transform = {
                .matrix = { {
                    instance.world[0].x, instance.world[1].x, instance.world[2].x, instance.world[3].x,
                    instance.world[1].y, instance.world[1].y, instance.world[2].y, instance.world[3].y,
                    instance.world[0].z, instance.world[1].z, instance.world[2].z, instance.world[3].z
                } }
            };

            vk::DeviceAddress blasAddress;
            vk::AccelerationStructureDeviceAddressInfoKHR addressInfo = {
                .accelerationStructure = m_blas[instance.objectId].as
            };
            blasAddress = m_vkContext.m_device.getAccelerationStructureAddressKHR(addressInfo);
            
            vk::AccelerationStructureInstanceKHR rayInstance {
                .transform = transform, // row-major matrix
                .instanceCustomIndex = instance.objectId,
                .mask = 0xFF,
                .instanceShaderBindingTableRecordOffset = 0,
                .flags = static_cast<uint8_t>(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable),
                .accelerationStructureReference = blasAddress
            };

            tlas.emplace_back(rayInstance);
        }

        buildTlas(tlas, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
    }

    void initVulkan() {
        m_vkContext.init("test", m_pWindow);

        m_pResourceManager = std::make_shared<ResourceManager>(&m_vkContext);
        m_pScene = std::make_shared<Scene>();

        m_swapChainFramebuffers = std::make_shared<std::vector<vk::Framebuffer>>();
        m_swapChainColorImages = std::make_shared<std::vector<vk::Image>>();
        m_swapChainColorImageViews = std::make_shared<std::vector<vk::ImageView>>();

        // to fix: strange copy construction
        OffscreenRenderPass o(&m_vkContext, m_commandPool, m_pResourceManager, m_pScene);
        FinalRenderPass f(&m_vkContext, m_commandPool, m_pResourceManager, m_pScene);
        offscreenRenderPass = o;
        finalRenderPass = f;

        createSwapChain();
        createSwapChainImageViews();
        m_pResourceManager->setExtent(m_swapChainExtent);
        offscreenRenderPass.setExtent(m_swapChainExtent);
        finalRenderPass.setExtent(m_swapChainExtent);
        createCommandPool();
        m_pResourceManager->setCommandPool(m_commandPool);

        m_pResourceManager->createUniformBuffer("CameraBuffer");
        m_pResourceManager->createModelTexture("ModelTexture", "textures/viking_room.png");
        auto object = m_pResourceManager->loadObjModel("Room", "models/viking_room.obj");
        m_pScene->addObject(object);

        createRandomInstances();

        offscreenRenderPass.setup();

        finalRenderPass.setSwapChainImagePointers(m_swapChainFramebuffers, m_swapChainColorImages, m_swapChainColorImageViews);
        finalRenderPass.setup();

        createCommandBuffers();
        createSyncObjects();
    }

    void mainLoop() {
        Timer timer;

        while (!glfwWindowShouldClose(m_pWindow)) {
            float deltaTime = static_cast<float>(timer.elapsed());
            glfwPollEvents();
            
            updateGUI(deltaTime);
            // updateScene(deltaTime)

            drawFrame();
        }

        m_vkContext.m_device.waitIdle();
    }

    void cleanup() {
        ImGui_ImplVulkan_Shutdown();
        m_vkContext.m_device.destroyDescriptorPool(m_imguiDescriptorPool, nullptr);
        
        offscreenRenderPass.cleanup();
        finalRenderPass.cleanup();

        m_pResourceManager->destroyTextures();
        m_pResourceManager->destroyBuffers();

        cleanupSwapChain();

        m_vkContext.m_device.destroySemaphore(m_imageAvailableSemaphore, nullptr);
        m_vkContext.m_device.destroySemaphore(m_renderFinishedSemaphore, nullptr);
        m_vkContext.m_device.destroyFence(m_inFlightFence, nullptr);

        m_vkContext.m_device.destroyCommandPool(m_commandPool, nullptr);

        m_vkContext.cleanup();

        glfwDestroyWindow(m_pWindow);
        glfwTerminate();
    }

    void createRandomInstances() {
        std::default_random_engine rng((unsigned)time(nullptr));
        std::uniform_real_distribution<float> uniformDist(0.0, 1.0);
        std::uniform_real_distribution<float> uniformDistPos(-1.0, 1.0);
        for (uint32_t i = 0; i < kInstanceCount; ++i) {
            ObjectInstance instance;
            
            auto pos = glm::translate(glm::identity<glm::mat4>(), glm::vec3(uniformDistPos(rng), uniformDistPos(rng), uniformDistPos(rng)));
            auto scale = glm::scale(glm::identity<glm::mat4>(), glm::vec3(0.5f, 0.5f, 0.5f));
            auto rot = glm::rotate(glm::identity<glm::mat4>(), uniformDist(rng) * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            
            // glsl and glm: uses column-major order matrices of column vectors
            instance.world = pos * rot * scale;
            instance.invTransposeWorld = glm::transpose(glm::inverse(instance.world));
            instance.objectId = 0;
            
            m_instances.push_back(instance);
        }

        m_pScene->setInstanceCount(m_instances.size());

        Buffer buffer;

        vk::DeviceSize bufferSize = m_instances.size() * sizeof(ObjectInstance);

        // staging buffer
        vk::Buffer stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;
        createBuffer(m_vkContext, bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

        void* data;
        data = m_vkContext.m_device.mapMemory(stagingBufferMemory, 0, bufferSize);
            memcpy(data, m_instances.data(), (size_t)bufferSize);
        m_vkContext.m_device.unmapMemory(stagingBufferMemory);

        // instance buffer
        vk::Buffer instanceBuffer;
        vk::DeviceMemory instanceBufferMemory;
        createBuffer(m_vkContext, bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR, vk::MemoryPropertyFlagBits::eDeviceLocal, instanceBuffer, instanceBufferMemory);

        // copy to staging buffer
        copyBuffer(m_vkContext, m_commandPool, stagingBuffer, instanceBuffer, bufferSize);

        m_vkContext.m_device.destroyBuffer(stagingBuffer, nullptr);
        m_vkContext.m_device.freeMemory(stagingBufferMemory, nullptr);

        buffer.descriptorInfo.buffer = instanceBuffer;
        buffer.memory = instanceBufferMemory;
        m_pResourceManager->insertBuffer("InstanceBuffer", buffer);
    }

    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = m_vkContext.querySwapChainSupport(m_vkContext.m_physicalDevice);

        vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        m_imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && m_imageCount > swapChainSupport.capabilities.maxImageCount) {
            m_imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        vk::SwapchainCreateInfoKHR createInfo { .surface = m_vkContext.m_surface,
                                                .minImageCount = m_imageCount,
                                                .imageFormat = surfaceFormat.format,
                                                .imageColorSpace = surfaceFormat.colorSpace,
                                                .imageExtent = extent,
                                                .imageArrayLayers = 1,
                                                .imageUsage = vk::ImageUsageFlagBits::eColorAttachment };
        
        QueueFamilyIndices indices = m_vkContext.findQueueFamilies(m_vkContext.m_physicalDevice);
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else {
            createInfo.imageSharingMode = vk::SharingMode::eExclusive;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (m_vkContext.m_device.createSwapchainKHR(&createInfo, nullptr, &m_swapChain) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create swap chain!");
        }

        if (m_vkContext.m_device.getSwapchainImagesKHR(m_swapChain, &m_imageCount, nullptr) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to get swap chain imageCount!");
        }
        m_swapChainColorImages->resize(m_imageCount);
        if (m_vkContext.m_device.getSwapchainImagesKHR(m_swapChain, &m_imageCount, m_swapChainColorImages->data()) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to get swap chain!");
        }
        m_swapChainImageFormat = surfaceFormat.format;
        m_swapChainExtent = extent;
    }

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
                availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == vk::PresentModeKHR::eImmediate) {
                return availablePresentMode;
            }
        }
        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        else {
            int width, height;
            glfwGetFramebufferSize(m_pWindow, &width, &height);

            vk::Extent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
            return actualExtent;
        }
    }

    void createSwapChainImageViews() {
        m_swapChainColorImageViews->resize(m_swapChainColorImages->size());

        for (size_t i = 0; i < m_swapChainColorImageViews->size(); ++i) {
            vk::ImageViewCreateInfo viewInfo { 
                .image = (*m_swapChainColorImages)[i],
                .viewType = vk::ImageViewType::e2D,
                .format = m_swapChainImageFormat,
                .subresourceRange = { 
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1 } 
            };

            if (m_vkContext.m_device.createImageView(&viewInfo, nullptr, &(*m_swapChainColorImageViews)[i]) != vk::Result::eSuccess) {
                throw std::runtime_error("failed to create swap chain image view!");
            }
        }
    }

    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = m_vkContext.findQueueFamilies(m_vkContext.m_physicalDevice);

        vk::CommandPoolCreateInfo poolInfo { .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                             .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value() };
        
        if (m_vkContext.m_device.createCommandPool(&poolInfo, nullptr, &m_commandPool) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void createCommandBuffers() {
        m_commandBuffers.resize(1);

        vk::CommandBufferAllocateInfo allocInfo { 
            .commandPool = m_commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = (uint32_t) m_commandBuffers.size()
        };
        
        if (m_vkContext.m_device.allocateCommandBuffers(&allocInfo, m_commandBuffers.data()) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex) {
        vk::CommandBufferBeginInfo beginInfo { 
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
            .pInheritanceInfo = nullptr
        };
        
        if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        updateUniformBuffer("CameraBuffer");
        offscreenRenderPass.record(commandBuffer);

        vk::ImageMemoryBarrier barrier { 
            .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = m_pResourceManager->getTexture(kOffscreenOutputTextureNames[kCurrentItem]).image,
            .subresourceRange = { 
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1 } 
        };
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        vk::PipelineStageFlags sourceStage = vk::PipelineStageFlagBits::eAllCommands;
        vk::PipelineStageFlags destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
        commandBuffer.pipelineBarrier(sourceStage, destinationStage, 
                                      {},
                                      0, nullptr, 
                                      0, nullptr,
                                      1, &barrier);

        finalRenderPass.record(commandBuffer);

        try { 
            commandBuffer.end();
        } catch (vk::SystemError err) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void createSyncObjects() {
        vk::SemaphoreCreateInfo semaphoreInfo{};
        vk::FenceCreateInfo fenceInfo{ 
            .flags = vk::FenceCreateFlagBits::eSignaled 
        };

        if (m_vkContext.m_device.createSemaphore(&semaphoreInfo, nullptr, &m_imageAvailableSemaphore) != vk::Result::eSuccess ||
            m_vkContext.m_device.createSemaphore(&semaphoreInfo, nullptr, &m_renderFinishedSemaphore) != vk::Result::eSuccess ||
            m_vkContext.m_device.createFence(&fenceInfo, nullptr, &m_inFlightFence) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }

    void drawFrame() {
        vk::Result result;

        // at the start of the frame, we want to wait until the previous frame has finished
        do {
            result = m_vkContext.m_device.waitForFences(1, &m_inFlightFence, VK_TRUE, UINT64_MAX);
        } while (result == vk::Result::eTimeout);

        // change the descriptor sets w.r.t. updated gui (e.g., output buffer)
        if (kDirty) {
            finalRenderPass.updateDescriptorSets();
            kDirty = false;
        }

        uint32_t imageIndex;
        // "...to be signaled when the presentation engine is finished using the image. 
        // That's the point in time where we can start drawing to it."
        result = m_vkContext.m_device.acquireNextImageKHR(m_swapChain, UINT64_MAX, m_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        if (result == vk::Result::eErrorOutOfDateKHR) {
            recreateSwapChain();
            return;
        } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }
        
        finalRenderPass.updateSwapChainImageIndex(imageIndex);

        // only reset the fence if we are submitting work
        if (m_vkContext.m_device.resetFences(1, &m_inFlightFence) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to reset fence!");
        }

        // currently only one command buffer is used.
        m_commandBuffers[0].reset();
        recordCommandBuffer(m_commandBuffers[0], imageIndex);

        vk::Semaphore waitSemaphores[] = {m_imageAvailableSemaphore};
        vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
        vk::Semaphore signalSemaphores[] = {m_renderFinishedSemaphore};

        vk::SubmitInfo submitInfo { 
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = waitSemaphores,
            .pWaitDstStageMask = waitStages,
            .commandBufferCount = 1,
            .pCommandBuffers = &m_commandBuffers[0],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = signalSemaphores
        };
        
        if (m_vkContext.m_graphicsQueue.submit(1, &submitInfo, m_inFlightFence) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        vk::SwapchainKHR swapChains[] {m_swapChain};
        vk::PresentInfoKHR presentInfo { 
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = signalSemaphores,
            .swapchainCount = 1,
            .pSwapchains = swapChains,
            .pImageIndices = &imageIndex,
            .pResults = nullptr 
        };
        
        result = m_vkContext.m_presentQueue.presentKHR(&presentInfo);
    
        if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || m_framebufferResized) {
            m_framebufferResized = false;
            recreateSwapChain();
        } else if (result != vk::Result::eSuccess) {
            throw std::runtime_error("failed to present swap chain image!");
        }
    }

    void cleanupSwapChain() {        
        for (size_t i = 0; i < m_swapChainFramebuffers->size(); ++i) {
            m_vkContext.m_device.destroyFramebuffer((*m_swapChainFramebuffers)[i], nullptr);
        }

        for (size_t i = 0; i < m_swapChainColorImageViews->size(); ++i) {
            m_vkContext.m_device.destroyImageView((*m_swapChainColorImageViews)[i], nullptr);
        }

        m_vkContext.m_device.destroySwapchainKHR(m_swapChain, nullptr);
    }

    void recreateSwapChain() {
        int width = 0, height = 0;
        glfwGetFramebufferSize(m_pWindow, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(m_pWindow, &width, &height);
            glfwWaitEvents();
        }

        m_vkContext.m_device.waitIdle();

        cleanupSwapChain();
        // cleanupOffscreenResources();

        createSwapChain();
        createSwapChainImageViews();
        // createSwapChainFrameBuffers();

        // createOffscreenRender();
        // updateFinalDescriptorSets();
    }

    void updateUniformBuffer(std::string name) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        Camera ubo{};
        // ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(glm::vec3(3.0 * glm::cos(time * glm::radians(90.0f)), 3.0 * glm::sin(time * glm::radians(90.0f)), 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        // ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), m_swapChainExtent.width / (float)m_swapChainExtent.height, 0.1f, 10.0f);

        // GLM's Y coordinate of the clip coordinates is inverted
        // To compensate this, flip the sign on the scaling factor of the Y axis in the proj matrix.
        ubo.proj[1][1] *= -1;

        memcpy(m_pResourceManager->getMappedBuffer(name), &ubo, sizeof(ubo));
    }
};

} // namespace vrb

int main() {
    vrb::Application app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

