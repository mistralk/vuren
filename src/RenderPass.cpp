#include "RenderPass.hpp"
#include "VulkanContext.hpp"

namespace vuren {

// ------------------ RenderPass bass class ------------------

RenderPass::RenderPass() {}

RenderPass::~RenderPass() {}

void RenderPass::init(VulkanContext *pContext, vk::CommandPool commandPool,
                      std::shared_ptr<ResourceManager> pResourceManager, std::shared_ptr<Scene> pScene) {
    m_pContext         = pContext;
    m_commandPool      = commandPool;
    m_pResourceManager = pResourceManager;
    m_pScene           = pScene;
}

void RenderPass::cleanup() {
    if (m_descriptorPool)
        m_pContext->m_device.destroyDescriptorPool(m_descriptorPool, nullptr);
    if (m_descriptorSetLayout)
        m_pContext->m_device.destroyDescriptorSetLayout(m_descriptorSetLayout, nullptr);
    if (m_pipeline)
        m_pContext->m_device.destroyPipeline(m_pipeline, nullptr);
    if (m_pipelineLayout)
        m_pContext->m_device.destroyPipelineLayout(m_pipelineLayout, nullptr);
}

void RenderPass::createDescriptorSet(const std::vector<ResourceBindingInfo> &bindingInfos) {
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    uint32_t totalDescriptorCount = 0;

    // create a descriptor set
    for (const auto &binding: bindingInfos) {
        vk::DescriptorSetLayoutBinding layoutBinding{ .binding            = static_cast<uint32_t>(bindings.size()),
                                                      .descriptorType     = binding.descriptorType,
                                                      .descriptorCount    = binding.descriptorCount,
                                                      .stageFlags         = binding.stageFlags,
                                                      .pImmutableSamplers = nullptr };

        bindings.push_back(layoutBinding);

        totalDescriptorCount += binding.descriptorCount;
    }

    vk::DescriptorSetLayoutCreateInfo layoutInfo{ .bindingCount = static_cast<uint32_t>(bindings.size()),
                                                  .pBindings    = bindings.data() };

    if (m_pContext->m_device.createDescriptorSetLayout(&layoutInfo, nullptr, &m_descriptorSetLayout) !=
        vk::Result::eSuccess) {
        throw std::runtime_error("failed to create a descriptor set layout!");
    }

    // create a descriptor pool
    std::vector<vk::DescriptorPoolSize> poolSizes(bindings.size());
    for (size_t i = 0; i < poolSizes.size(); ++i) {
        poolSizes[i].type            = bindings[i].descriptorType;
        poolSizes[i].descriptorCount = bindings[i].descriptorCount;
    }

    vk::DescriptorPoolCreateInfo poolInfo{ .maxSets       = 1,
                                           .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                                           .pPoolSizes    = poolSizes.data() };

    if (m_pContext->m_device.createDescriptorPool(&poolInfo, nullptr, &m_descriptorPool) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create a descriptor pool!");
    }

    // write a descriptor set
    vk::DescriptorSetAllocateInfo allocInfo{ .descriptorPool     = m_descriptorPool,
                                             .descriptorSetCount = 1,
                                             .pSetLayouts        = &m_descriptorSetLayout };

    if (m_pContext->m_device.allocateDescriptorSets(&allocInfo, &m_descriptorSet) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to allocate offscreen descriptor sets!");
    }

    std::vector<vk::WriteDescriptorSet> descriptorWrites;
    std::vector<vk::DescriptorBufferInfo> bufferInfos;
    std::vector<vk::DescriptorImageInfo> imageInfos;
    std::vector<vk::WriteDescriptorSetAccelerationStructureKHR> tlasInfos;

    bufferInfos.reserve(totalDescriptorCount);
    imageInfos.reserve(totalDescriptorCount);
    tlasInfos.reserve(totalDescriptorCount);

    for (size_t i = 0; i < bindings.size(); ++i) {
        vk::WriteDescriptorSet write;
        vk::DescriptorBufferInfo bufferInfo;
        vk::DescriptorImageInfo imageInfo;

        write.pImageInfo  = nullptr;
        write.pBufferInfo = nullptr;
        write.pNext       = nullptr; // for TLAS

        // top-level acceleration structures
        if (bindings[i].descriptorType == vk::DescriptorType::eAccelerationStructureKHR) {
            vk::WriteDescriptorSetAccelerationStructureKHR descriptorSetAsInfo{ .accelerationStructureCount =
                                                                                    bindings[i].descriptorCount,
                                                                                .pAccelerationStructures = &m_tlas.as };
            tlasInfos.push_back(descriptorSetAsInfo);

            write.pNext            = (void *) &tlasInfos.back();
            write.dstSet           = m_descriptorSet;
            write.dstBinding       = static_cast<uint32_t>(i);
            write.dstArrayElement  = 0;
            write.descriptorCount  = bindings[i].descriptorCount;
            write.descriptorType   = bindings[i].descriptorType;
            write.pTexelBufferView = nullptr;

            descriptorWrites.push_back(write);
            continue;
        }

        // specialized bindings for scene resourses
        if (bindingInfos[i].name == "SceneTextures") {
            assert(bindings[i].descriptorType == vk::DescriptorType::eCombinedImageSampler);
            for (auto &texture: m_pScene->getTextures()) {
                imageInfo             = m_pResourceManager->getTexture(texture.name).descriptorInfo;
                imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                imageInfos.push_back(imageInfo);
            }
            write.pImageInfo = &imageInfos.back() - m_pScene->getTextures().size() + 1;
        } else if (bindingInfos[i].name == "SceneObjects") {
            assert(bindings[i].descriptorType == vk::DescriptorType::eStorageBuffer);
            for (auto &buffer: m_pScene->getObjects()) {
                bufferInfo = m_pResourceManager->getBuffer("SceneObjectDeviceInfo").descriptorInfo;
                bufferInfos.push_back(bufferInfo);
            }
            write.pBufferInfo = &bufferInfos.back() - m_pScene->getObjects().size() + 1;
        }

        // everything else resources
        else {
            switch (bindings[i].descriptorType) {
                case vk::DescriptorType::eCombinedImageSampler:
                    imageInfo             = m_pResourceManager->getTexture(bindingInfos[i].name).descriptorInfo;
                    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                    imageInfos.push_back(imageInfo);
                    write.pImageInfo = &imageInfos.back();
                    break;

                case vk::DescriptorType::eStorageImage:
                    imageInfo             = m_pResourceManager->getTexture(bindingInfos[i].name).descriptorInfo;
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
        }

        write.dstSet           = m_descriptorSet;
        write.dstBinding       = static_cast<uint32_t>(i);
        write.dstArrayElement  = 0;
        write.descriptorCount  = bindings[i].descriptorCount;
        write.descriptorType   = bindings[i].descriptorType;
        write.pTexelBufferView = nullptr;

        descriptorWrites.push_back(write);
    }

    m_pContext->m_device.updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(),
                                              0, nullptr);
}

vk::ShaderModule RenderPass::createShaderModule(const std::vector<char> &code) {
    vk::ShaderModuleCreateInfo createInfo{ .codeSize = code.size(),
                                           .pCode    = reinterpret_cast<const uint32_t *>(code.data()) };

    vk::ShaderModule shaderModule;
    if (m_pContext->m_device.createShaderModule(&createInfo, nullptr, &shaderModule) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

// ------------------ RasterRenderPass class ------------------

RasterRenderPass::RasterRenderPass() {}

RasterRenderPass::~RasterRenderPass() {}

void RasterRenderPass::init(VulkanContext *pContext, vk::CommandPool commandPool,
                            std::shared_ptr<ResourceManager> pResourceManager, std::shared_ptr<Scene> pScene) {
    RenderPass::init(pContext, commandPool, pResourceManager, pScene);
}

void RasterRenderPass::setup() { define(); }

void RasterRenderPass::cleanup() {
    if (m_framebuffer)
        m_pContext->m_device.destroyFramebuffer(m_framebuffer, nullptr);
    if (m_renderPass)
        m_pContext->m_device.destroyRenderPass(m_renderPass, nullptr);
    RenderPass::cleanup();
}

void RasterRenderPass::setupRasterPipeline(const std::string &vertShaderPath, const std::string &fragShaderPath,
                                           bool isBlitPass) {
    if (!m_pContext->m_device || !m_descriptorSetLayout) {
        throw std::runtime_error(
            "pipeline setup failed! "
            "logical device and descriptor set layout must be valid before the pipeline creation.");
    }

    m_vertShaderPath = vertShaderPath;
    m_fragShaderPath = fragShaderPath;
    m_isBiltPass     = isBlitPass;

    // create a pipeline layout
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{ .setLayoutCount         = 1,
                                                     .pSetLayouts            = &m_descriptorSetLayout,
                                                     .pushConstantRangeCount = 0,
                                                     .pPushConstantRanges    = nullptr };

    if (m_pContext->m_device.createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_pipelineLayout) !=
        vk::Result::eSuccess) {
        throw std::runtime_error("failed to create a pipeline layout!");
    }

    auto vertShaderCode = readFile(m_vertShaderPath);
    auto fragShaderCode = readFile(m_fragShaderPath);

    vk::ShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    vk::ShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{ .stage  = vk::ShaderStageFlagBits::eVertex,
                                                           .module = vertShaderModule,
                                                           .pName  = "main" };

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{ .stage  = vk::ShaderStageFlagBits::eFragment,
                                                           .module = fragShaderModule,
                                                           .pName  = "main" };

    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    // Fixed function: Vertex input
    // per-vertex data
    auto vertexAttributeDescription = Vertex::getAttributeDescriptions();

    // per-instance data
    auto instanceAttributeDescription = ObjectInstance::getAttributeDescriptions();

    std::vector<vk::VertexInputBindingDescription> bindingDescriptions = {
        { 0, sizeof(Vertex), vk::VertexInputRate::eVertex },
        { 1, sizeof(ObjectInstance), vk::VertexInputRate::eInstance }
    };

    for (uint32_t i = 0; i < vertexAttributeDescription.size(); ++i)
        vertexAttributeDescription[i].binding = 0;

    for (uint32_t i = 0; i < instanceAttributeDescription.size(); ++i)
        instanceAttributeDescription[i].binding = 1;

    // merge all vertex input data
    std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
    attributeDescriptions.insert(attributeDescriptions.end(), vertexAttributeDescription.begin(),
                                 vertexAttributeDescription.end());
    attributeDescriptions.insert(attributeDescriptions.end(), instanceAttributeDescription.begin(),
                                 instanceAttributeDescription.end());

    for (uint32_t i = 0; i < attributeDescriptions.size(); ++i)
        attributeDescriptions[i].location = i;

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    if (m_isBiltPass) {
        vertexInputInfo.vertexBindingDescriptionCount   = 0;
        vertexInputInfo.pVertexBindingDescriptions      = nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions    = nullptr;
    } else {
        vertexInputInfo.vertexBindingDescriptionCount   = static_cast<uint32_t>(bindingDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions      = bindingDescriptions.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();
    }

    // Fixed function: Input assembly
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{ .topology = vk::PrimitiveTopology::eTriangleList,
                                                            .primitiveRestartEnable = VK_FALSE };

    // Fixed function: Viewport and scissor
    std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

    vk::PipelineDynamicStateCreateInfo dynamicState{ .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                                                     .pDynamicStates    = dynamicStates.data() };

    vk::PipelineViewportStateCreateInfo viewportState{ .viewportCount = 1, .scissorCount = 1 };

    // Fixed function: Rasterizer
    vk::PipelineRasterizationStateCreateInfo rasterizer{ .depthClampEnable        = VK_FALSE,
                                                         .rasterizerDiscardEnable = VK_FALSE,
                                                         .polygonMode             = vk::PolygonMode::eFill,
                                                         .cullMode        = m_isBiltPass ? vk::CullModeFlagBits::eNone
                                                                                         : vk::CullModeFlagBits::eBack,
                                                         .frontFace       = vk::FrontFace::eCounterClockwise,
                                                         .depthBiasEnable = VK_FALSE,
                                                         .depthBiasConstantFactor = 0.0f,
                                                         .depthBiasClamp          = 0.0f,
                                                         .depthBiasSlopeFactor    = 0.0f,
                                                         .lineWidth               = 1.0f };

    // Fixed function: Multisampling
    vk::PipelineMultisampleStateCreateInfo multisampling{ .rasterizationSamples  = vk::SampleCountFlagBits::e1,
                                                          .sampleShadingEnable   = VK_FALSE,
                                                          .minSampleShading      = 1.0f,
                                                          .pSampleMask           = nullptr,
                                                          .alphaToCoverageEnable = VK_FALSE,
                                                          .alphaToOneEnable      = VK_FALSE };

    // Fixed function: Depth and stencil testing
    vk::PipelineDepthStencilStateCreateInfo depthStencil{ .depthTestEnable       = VK_TRUE,
                                                          .depthWriteEnable      = VK_TRUE,
                                                          .depthCompareOp        = vk::CompareOp::eLess,
                                                          .depthBoundsTestEnable = VK_FALSE,
                                                          .stencilTestEnable     = VK_FALSE,
                                                          .front                 = {},
                                                          .back                  = {},
                                                          .minDepthBounds        = 0.0f,
                                                          .maxDepthBounds        = 1.0f };

    // Fixed function: Color blending
    // default setup for color attachments
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{ .blendEnable         = VK_FALSE,
                                                                .srcColorBlendFactor = vk::BlendFactor::eOne,
                                                                .dstColorBlendFactor = vk::BlendFactor::eZero,
                                                                .colorBlendOp        = vk::BlendOp::eAdd,
                                                                .srcAlphaBlendFactor = vk::BlendFactor::eOne,
                                                                .dstAlphaBlendFactor = vk::BlendFactor::eZero,
                                                                .alphaBlendOp        = vk::BlendOp::eAdd,
                                                                .colorWriteMask      = vk::ColorComponentFlagBits::eR |
                                                                                  vk::ColorComponentFlagBits::eG |
                                                                                  vk::ColorComponentFlagBits::eB |
                                                                                  vk::ColorComponentFlagBits::eA };

    std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments(m_colorAttachmentCount,
                                                                             colorBlendAttachment);

    vk::PipelineColorBlendStateCreateInfo colorBlending{ .logicOpEnable   = VK_FALSE,
                                                         .logicOp         = vk::LogicOp::eCopy,
                                                         .attachmentCount = m_colorAttachmentCount,
                                                         .pAttachments    = colorBlendAttachments.data(),
                                                         .blendConstants =
                                                             std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f } };

    vk::GraphicsPipelineCreateInfo pipelineInfo{ .stageCount          = 2,
                                                 .pStages             = shaderStages,
                                                 .pVertexInputState   = &vertexInputInfo,
                                                 .pInputAssemblyState = &inputAssembly,
                                                 .pViewportState      = &viewportState,
                                                 .pRasterizationState = &rasterizer,
                                                 .pMultisampleState   = &multisampling,
                                                 .pDepthStencilState  = &depthStencil,
                                                 .pColorBlendState    = &colorBlending,
                                                 .pDynamicState       = &dynamicState,
                                                 .layout              = m_pipelineLayout,
                                                 .renderPass          = m_renderPass,
                                                 .subpass             = 0,
                                                 .basePipelineHandle  = VK_NULL_HANDLE,
                                                 .basePipelineIndex   = -1 };

    if (m_pContext->m_device.createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) !=
        vk::Result::eSuccess) {
        throw std::runtime_error("failed to create rasterization pipeline!");
    }

    m_pContext->m_device.destroyShaderModule(fragShaderModule, nullptr);
    m_pContext->m_device.destroyShaderModule(vertShaderModule, nullptr);
}

void RasterRenderPass::createVkRenderPass(const std::vector<AttachmentInfo> &colorAttachmentInfos,
                                          const AttachmentInfo &depthStencilAttachmentInfo) {
    if (m_renderPass) {
        return;
    }

    std::vector<vk::AttachmentDescription> attachments;
    std::vector<vk::AttachmentReference> colorAttachmentRefs;
    vk::PipelineStageFlags srcStageMask = vk::PipelineStageFlagBits::eNone;
    vk::PipelineStageFlags dstStageMask = vk::PipelineStageFlagBits::eNone;
    vk::AccessFlags srcAccessMask       = vk::AccessFlagBits::eNone;
    vk::AccessFlags dstAccessMask       = vk::AccessFlagBits::eNone;

    // color attachments
    for (const auto &attachmentInfo: colorAttachmentInfos) {
        vk::AttachmentDescription colorAttachment{ .format         = attachmentInfo.format,
                                                   .samples        = vk::SampleCountFlagBits::e1,
                                                   .loadOp         = vk::AttachmentLoadOp::eClear,
                                                   .storeOp        = vk::AttachmentStoreOp::eStore,
                                                   .stencilLoadOp  = vk::AttachmentLoadOp::eDontCare,
                                                   .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                                                   .initialLayout  = attachmentInfo.oldLayout,
                                                   .finalLayout    = attachmentInfo.newLayout };

        vk::AttachmentReference colorAttachmentRef{ .attachment = static_cast<uint32_t>(attachments.size()),
                                                    .layout =
                                                        attachmentInfo.newLayout == vk::ImageLayout::ePresentSrcKHR
                                                            ? vk::ImageLayout::eColorAttachmentOptimal
                                                            : attachmentInfo.newLayout };

        srcStageMask |= attachmentInfo.srcStageMask;
        dstStageMask |= attachmentInfo.dstStageMask;
        srcAccessMask |= attachmentInfo.srcAccessMask;
        dstAccessMask |= attachmentInfo.dstAccessMask;

        attachments.push_back(colorAttachment);
        colorAttachmentRefs.push_back(colorAttachmentRef);
    }

    // depth stencil attachment
    vk::AttachmentDescription depthAttachment{ .format         = depthStencilAttachmentInfo.format,
                                               .samples        = vk::SampleCountFlagBits::e1,
                                               .loadOp         = vk::AttachmentLoadOp::eClear,
                                               .storeOp        = vk::AttachmentStoreOp::eDontCare,
                                               .stencilLoadOp  = vk::AttachmentLoadOp::eDontCare,
                                               .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                                               .initialLayout  = depthStencilAttachmentInfo.oldLayout,
                                               .finalLayout    = depthStencilAttachmentInfo.newLayout };

    vk::AttachmentReference depthAttachmentRef{ .attachment = static_cast<uint32_t>(attachments.size()),
                                                .layout     = depthStencilAttachmentInfo.newLayout };

    attachments.push_back(depthAttachment);

    vk::SubpassDescription subpass = { .pipelineBindPoint       = vk::PipelineBindPoint::eGraphics,
                                       .colorAttachmentCount    = static_cast<uint32_t>(colorAttachmentRefs.size()),
                                       .pColorAttachments       = colorAttachmentRefs.data(),
                                       .pDepthStencilAttachment = &depthAttachmentRef };

    // vk::SubpassDependency dependency {
    //     .srcSubpass = VK_SUBPASS_EXTERNAL,
    //     .dstSubpass = 0,
    //     .srcStageMask = srcStageMask,
    //     .dstStageMask = dstStageMask,
    //     .srcAccessMask = srcAccessMask,
    //     .dstAccessMask = dstAccessMask
    // };

    vk::SubpassDependency dependency{ .srcSubpass    = VK_SUBPASS_EXTERNAL,
                                      .dstSubpass    = 0,
                                      .srcStageMask  = vk::PipelineStageFlagBits::eFragmentShader,
                                      .dstStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                      .srcAccessMask = vk::AccessFlagBits::eShaderRead,
                                      .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite };

    vk::RenderPassCreateInfo renderPassInfo{ .attachmentCount = static_cast<uint32_t>(attachments.size()),
                                             .pAttachments    = attachments.data(),
                                             .subpassCount    = 1,
                                             .pSubpasses      = &subpass,
                                             .dependencyCount = 1,
                                             .pDependencies   = &dependency };

    if (m_pContext->m_device.createRenderPass(&renderPassInfo, nullptr, &m_renderPass) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void RasterRenderPass::createFramebuffer(const std::vector<AttachmentInfo> &colorAttachmentInfos,
                                         const AttachmentInfo &depthStencilAttachmentInfo) {
    std::vector<vk::ImageView> attachments;
    for (const auto &attachment: colorAttachmentInfos) {
        attachments.push_back(attachment.imageView);
    }
    attachments.push_back(depthStencilAttachmentInfo.imageView);

    vk::FramebufferCreateInfo framebufferInfo{ .renderPass      = m_renderPass,
                                               .attachmentCount = static_cast<uint32_t>(attachments.size()),
                                               .pAttachments    = attachments.data(),
                                               .width           = m_extent.width,
                                               .height          = m_extent.height,
                                               .layers          = 1 };

    if (m_pContext->m_device.createFramebuffer(&framebufferInfo, nullptr, &m_framebuffer) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create framebuffer!");
    }

    m_colorAttachmentCount = static_cast<uint32_t>(colorAttachmentInfos.size());
}

// ------------------ RayTracingRenderPass class ------------------

RayTracingRenderPass::RayTracingRenderPass() {}

RayTracingRenderPass::~RayTracingRenderPass() {}

void RayTracingRenderPass::init(VulkanContext *pContext, vk::CommandPool commandPool,
                                std::shared_ptr<ResourceManager> pResourceManager, std::shared_ptr<Scene> pScene) {
    RenderPass::init(pContext, commandPool, pResourceManager, pScene);

    vk::PhysicalDeviceProperties2 prop2{ .pNext = &m_rtProperties };
    m_pContext->m_physicalDevice.getProperties2(&prop2);

    createBlas();
    createTlas(m_pScene->getInstances());
}

void RayTracingRenderPass::setup() {
    define();
    createShaderBindingTable();
}

void RayTracingRenderPass::cleanup() {
    m_pResourceManager->destroyBuffer(m_sbtBuffer);
    m_pResourceManager->destroyBuffer(m_tlas.buffer);
    if (m_tlas.as)
        m_pContext->m_device.destroyAccelerationStructureKHR(m_tlas.as);
    for (auto &blas: m_blas) {
        if (blas.as) {
            m_pContext->m_device.destroyAccelerationStructureKHR(blas.as);
        }
        m_pResourceManager->destroyBuffer(blas.buffer);
    }
    RenderPass::cleanup();
}

RayTracingRenderPass::BlasInput RayTracingRenderPass::objectToVkGeometryKHR(const SceneObject &object) {
    // only the position attribute is needed for the AS build.
    // if position is not the first member of Vertex,
    // we have to manually adjust vertexAddress using offsetof.
    vk::DeviceAddress vertexAddress = m_pContext->getBufferDeviceAddress(object.vertexBuffer->descriptorInfo.buffer);
    vk::DeviceAddress indexAddress  = m_pContext->getBufferDeviceAddress(object.indexBuffer->descriptorInfo.buffer);

    uint32_t maxPrimitiveCount = object.indexBufferSize / 3;

    vk::AccelerationStructureGeometryTrianglesDataKHR triangles{
        // describe buffer as array of vertexobj
        .vertexFormat = vk::Format::eR32G32B32Sfloat,
        .vertexData   = { .deviceAddress = vertexAddress },
        .vertexStride = sizeof(Vertex),
        .maxVertex    = object.vertexBufferSize,
        // describe index data
        .indexType     = vk::IndexType::eUint32,
        .indexData     = { .deviceAddress = indexAddress },
        .transformData = {} // identity transform
    };

    vk::AccelerationStructureGeometryKHR asGeom{ .geometryType = vk::GeometryTypeKHR::eTriangles,
                                                 .geometry     = { .triangles = triangles },
                                                 .flags        = vk::GeometryFlagBitsKHR::eOpaque };

    vk::AccelerationStructureBuildRangeInfoKHR offset{
        .primitiveCount = maxPrimitiveCount, .primitiveOffset = 0, .firstVertex = 0, .transformOffset = 0
    };

    BlasInput input;
    input.asGeometry.emplace_back(asGeom);
    input.asBuildOffsetInfo.emplace_back(offset);

    return input;
}

// generate one BLAS for each BlasInput
void RayTracingRenderPass::buildBlas(const std::vector<BlasInput> &input,
                                     vk::BuildAccelerationStructureFlagsKHR flags) {

    uint32_t blasCount            = static_cast<uint32_t>(input.size());
    vk::DeviceSize asTotalSize    = 0; // all aloocated BLAS
    uint32_t compactionsSize      = 0; // BLAS requesting compaction
    vk::DeviceSize maxScratchSize = 0;

    std::vector<BuildAccelerationStructure> buildAs(blasCount);

    // prepare the information for the acceleration build commands
    for (uint32_t i = 0; i < blasCount; ++i) {
        buildAs[i].buildInfo = { .type          = vk::AccelerationStructureTypeKHR::eBottomLevel,
                                 .flags         = input[i].flags | flags,
                                 .mode          = vk::BuildAccelerationStructureModeKHR::eBuild,
                                 .geometryCount = static_cast<uint32_t>(input[i].asGeometry.size()),
                                 .pGeometries   = input[i].asGeometry.data() };

        buildAs[i].rangeInfo = input[i].asBuildOffsetInfo.data();

        // find sizes to create acceleration structure and scratch
        std::vector<uint32_t> maxPrimCount(input[i].asBuildOffsetInfo.size());
        for (auto tt = 0; tt < input[i].asBuildOffsetInfo.size(); ++tt)
            maxPrimCount[tt] = input[i].asBuildOffsetInfo[tt].primitiveCount;

        m_pContext->m_device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice,
                                                                   &buildAs[i].buildInfo, maxPrimCount.data(),
                                                                   &buildAs[i].sizeInfo);

        asTotalSize += buildAs[i].sizeInfo.accelerationStructureSize;
        maxScratchSize = std::max(maxScratchSize, buildAs[i].sizeInfo.buildScratchSize);
        if ((buildAs[i].buildInfo.flags & vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction) ==
            vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction)
            compactionsSize += 1;
    }

    // allocate the "largest" scratch buffer holding the temporary data of the
    // acceleration structure builder
    Buffer scratchBuffer = m_pResourceManager->createBuffer(
        maxScratchSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    vk::DeviceAddress scratchAddress = m_pContext->getBufferDeviceAddress(scratchBuffer.descriptorInfo.buffer);

    vk::QueryPool queryPool{ VK_NULL_HANDLE };

    if (compactionsSize > 0) {
        assert(compactionsSize == blasCount);
        vk::QueryPoolCreateInfo poolCreateInfo = { .queryType  = vk::QueryType::eAccelerationStructureCompactedSizeKHR,
                                                   .queryCount = blasCount };
        if (m_pContext->m_device.createQueryPool(&poolCreateInfo, nullptr, &queryPool) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create a query pool!");
        }
    }

    // batching creation/compaction of BLAS to allow staying in restricted
    // amount of memory
    std::vector<uint32_t> indices;
    vk::DeviceSize batchSize  = 0;
    vk::DeviceSize batchLimit = 256'000'000;

    for (uint32_t i = 0; i < blasCount; ++i) {
        indices.push_back(i);
        batchSize += buildAs[i].sizeInfo.accelerationStructureSize;

        // over the limit or last BLAS element
        if (batchSize >= batchLimit || i == blasCount - 1) {
            // create a command buffer
            vk::CommandBuffer commandBuffer = beginSingleTimeCommands(*m_pContext, m_commandPool);

            // create BLAS
            if (queryPool)
                m_pContext->m_device.resetQueryPool(queryPool, 0, static_cast<uint32_t>(indices.size()));
            uint32_t queryCount = 0;

            for (const auto &j: indices) {
                vk::AccelerationStructureCreateInfoKHR createInfo{ .size =
                                                                       buildAs[j].sizeInfo.accelerationStructureSize,
                                                                   .type =
                                                                       vk::AccelerationStructureTypeKHR::eBottomLevel };

                AccelerationStructure as;
                m_pResourceManager->createAs(createInfo, as);
                buildAs[j].as                                  = as;
                buildAs[j].buildInfo.dstAccelerationStructure  = buildAs[j].as.as;
                buildAs[j].buildInfo.scratchData.deviceAddress = scratchAddress;

                commandBuffer.buildAccelerationStructuresKHR(1, &buildAs[j].buildInfo, &buildAs[j].rangeInfo);

                vk::MemoryBarrier barrier{ .srcAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR,
                                           .dstAccessMask = vk::AccessFlagBits::eAccelerationStructureReadKHR };

                commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, // src
                                              vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, // dst
                                              {}, 1, &barrier, 0, nullptr, 0, nullptr);

                if (queryPool) {
                    // add a query to find real amount of memory needed, use for
                    // compaction
                    commandBuffer.writeAccelerationStructuresPropertiesKHR(
                        1, &buildAs[j].buildInfo.dstAccelerationStructure,
                        vk::QueryType::eAccelerationStructureCompactedSizeKHR, queryPool, queryCount++);
                }
            }

            // submit and wait
            endSingleTimeCommands(*m_pContext, m_commandPool, commandBuffer);

            if (queryPool) {
                vk::CommandBuffer commandBuffer = beginSingleTimeCommands(*m_pContext, m_commandPool);

                // compact BLAS

                uint32_t queryCount = 0;
                std::vector<AccelerationStructure> cleanupAs; // previous AS to destroy

                // get the compacted size result back
                std::vector<vk::DeviceSize> compactSizes(static_cast<uint32_t>(indices.size()));
                if (m_pContext->m_device.getQueryPoolResults(queryPool, 0, (uint32_t) compactSizes.size(),
                                                             compactSizes.size() * sizeof(vk::DeviceSize),
                                                             compactSizes.data(), sizeof(vk::DeviceSize),
                                                             vk::QueryResultFlagBits::eWait) != vk::Result::eSuccess) {
                    throw std::runtime_error("failed to get query pool results!");
                }

                for (auto idx: indices) {
                    buildAs[idx].cleanupAs                          = buildAs[idx].as;
                    buildAs[idx].sizeInfo.accelerationStructureSize = compactSizes[queryCount++];

                    // create a compact version of the AS
                    vk::AccelerationStructureCreateInfoKHR asCreateInfo{
                        .size = buildAs[idx].sizeInfo.accelerationStructureSize,
                        .type = vk::AccelerationStructureTypeKHR::eBottomLevel
                    };
                    m_pResourceManager->createAs(asCreateInfo, buildAs[idx].as);

                    // copy the original BLAS to a compact version
                    vk::CopyAccelerationStructureInfoKHR copyInfo{ .src =
                                                                       buildAs[idx].buildInfo.dstAccelerationStructure,
                                                                   .dst = buildAs[idx].as.as,
                                                                   .mode =
                                                                       vk::CopyAccelerationStructureModeKHR::eCompact };
                    commandBuffer.copyAccelerationStructureKHR(&copyInfo);
                }

                // submit and wait
                endSingleTimeCommands(*m_pContext, m_commandPool, commandBuffer);

                // destroyNonCompacted
                for (auto &i: indices) {
                    m_pResourceManager->destroyAs(buildAs[i].cleanupAs);
                }
            }

            batchSize = 0;
            indices.clear();
        }
    }

    for (auto &b: buildAs) {
        m_blas.emplace_back(b.as);
    }

    m_pContext->m_device.destroyQueryPool(queryPool, nullptr);
    m_pResourceManager->destroyBuffer(scratchBuffer);
}

void RayTracingRenderPass::createBlas() {
    // BLAS stores each primitive in a geometry.
    std::vector<BlasInput> allBlas;
    allBlas.reserve(m_pScene->getObjects().size());

    for (const auto &obj: m_pScene->getObjects()) {
        auto blas = objectToVkGeometryKHR(obj);
        allBlas.emplace_back(blas);
    }

    buildBlas(allBlas, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
}

void RayTracingRenderPass::buildTlas(const std::vector<vk::AccelerationStructureInstanceKHR> &instances,
                                     vk::BuildAccelerationStructureFlagsKHR flags, bool update) {
    assert(m_tlas.as == vk::AccelerationStructureKHR{ VK_NULL_HANDLE } || update);
    uint32_t instanceCount = static_cast<uint32_t>(instances.size());

    vk::CommandBuffer commandBuffer = beginSingleTimeCommands(*m_pContext, m_commandPool);
    ;

    // caution: this is not the managed "InstanceBuffer"
    Buffer instanceBuffer = m_pResourceManager->createBufferByHostData<vk::AccelerationStructureInstanceKHR>(
        instances,
        vk::BufferUsageFlagBits::eShaderDeviceAddress |
            vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    vk::DeviceAddress instanceBufferAddress = m_pContext->getBufferDeviceAddress(instanceBuffer.descriptorInfo.buffer);

    // make sure the copy of the instance buffer are copied before triggering
    // the acceleration structure build
    vk::MemoryBarrier barrier{ .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                               .dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR };
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                  vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, {}, 1, &barrier, 0,
                                  nullptr, 0, nullptr);

    // create the TLAS
    vk::AccelerationStructureGeometryInstancesDataKHR instanceData;
    instanceData.data.deviceAddress = instanceBufferAddress;

    vk::AccelerationStructureGeometryKHR tlasGeometry;
    tlasGeometry.geometryType       = vk::GeometryTypeKHR::eInstances;
    tlasGeometry.geometry.instances = instanceData;

    vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{
        .type  = vk::AccelerationStructureTypeKHR::eTopLevel,
        .flags = flags,
        .mode = update ? vk::BuildAccelerationStructureModeKHR::eUpdate : vk::BuildAccelerationStructureModeKHR::eBuild,
        .srcAccelerationStructure = VK_NULL_HANDLE,
        .geometryCount            = 1,
        .pGeometries              = &tlasGeometry
    };

    vk::AccelerationStructureBuildSizesInfoKHR sizeInfo{};
    m_pContext->m_device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice,
                                                               &buildInfo, &instanceCount, &sizeInfo);

    vk::AccelerationStructureCreateInfoKHR createInfo{ .size = sizeInfo.accelerationStructureSize,
                                                       .type = vk::AccelerationStructureTypeKHR::eTopLevel };

    // create TLAS
    m_pResourceManager->createAs(createInfo, m_tlas);

    // allocate the scratch memory
    Buffer scratchBuffer             = m_pResourceManager->createBuffer(sizeInfo.buildScratchSize,
                                                                        vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                                                            vk::BufferUsageFlagBits::eStorageBuffer,
                                                                        vk::MemoryPropertyFlagBits::eDeviceLocal);
    vk::DeviceAddress scratchAddress = m_pContext->getBufferDeviceAddress(scratchBuffer.descriptorInfo.buffer);

    // update build information
    buildInfo.srcAccelerationStructure  = VK_NULL_HANDLE;
    buildInfo.dstAccelerationStructure  = m_tlas.as;
    buildInfo.scratchData.deviceAddress = scratchAddress;

    // build offsets info: n instances
    vk::AccelerationStructureBuildRangeInfoKHR offsetInfo{
        .primitiveCount = instanceCount, .primitiveOffset = 0, .firstVertex = 0, .transformOffset = 0
    };

    const vk::AccelerationStructureBuildRangeInfoKHR *pOffsetInfo = &offsetInfo;

    // build the TLAS
    commandBuffer.buildAccelerationStructuresKHR(1, &buildInfo, &pOffsetInfo);

    endSingleTimeCommands(*m_pContext, m_commandPool, commandBuffer);
    m_pResourceManager->destroyBuffer(scratchBuffer);
    m_pResourceManager->destroyBuffer(instanceBuffer);
}

void RayTracingRenderPass::createTlas(const std::vector<ObjectInstance> &instances) {
    // TLAS is the entry point in the rt scene description
    std::vector<vk::AccelerationStructureInstanceKHR> tlas;
    tlas.reserve(instances.size());

    for (const ObjectInstance &instance: instances) {
        // glm transform: column-major matrix
        // VkTransform: row-major matrix
        // we need to transpose it.
        vk::TransformMatrixKHR transform = {
            .matrix = { { instance.world[0].x, instance.world[1].x, instance.world[2].x, instance.world[3].x,
                          instance.world[0].y, instance.world[1].y, instance.world[2].y, instance.world[3].y,
                          instance.world[0].z, instance.world[1].z, instance.world[2].z, instance.world[3].z } }
        };

        vk::DeviceAddress blasAddress;
        vk::AccelerationStructureDeviceAddressInfoKHR addressInfo = { .accelerationStructure =
                                                                          m_blas[instance.objectId].as };
        blasAddress = m_pContext->m_device.getAccelerationStructureAddressKHR(addressInfo);

        vk::AccelerationStructureInstanceKHR rayInstance{
            .transform                              = transform,
            .instanceCustomIndex                    = instance.objectId,
            .mask                                   = 0xFF,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags = static_cast<uint8_t>(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable),
            .accelerationStructureReference = blasAddress
        };

        tlas.emplace_back(rayInstance);
    }

    buildTlas(tlas, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
}

uint32_t RayTracingRenderPass::align_up(uint32_t size, uint32_t alignment) {
    return (size + (alignment - 1)) & ~(alignment - 1);
}

void RayTracingRenderPass::createShaderBindingTable() {
    uint32_t missCount   = 1;
    uint32_t hitCount    = 1;
    uint32_t handleCount = 1 + missCount + hitCount;
    uint32_t handleSize  = m_rtProperties.shaderGroupHandleSize;

    // the SBT need to have starting groups to be aligned and handles in the
    // group to be aligned
    uint32_t handleSizeAligned = align_up(handleSize, m_rtProperties.shaderGroupHandleAlignment);

    m_rgenRegion.stride = align_up(handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);
    m_rgenRegion.size   = m_rgenRegion.stride; // pRayGenShaderBindingTable.size
                                               // must be equal to its stride
    m_missRegion.stride = handleSizeAligned;
    m_missRegion.size   = align_up(missCount * handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);
    m_hitRegion.stride  = handleSizeAligned;
    m_hitRegion.size    = align_up(hitCount * handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);

    // fetch the shader group handles of the pipeline
    uint32_t dataSize = handleCount * handleSize;
    std::vector<uint8_t> handles(dataSize);
    if (m_pContext->m_device.getRayTracingShaderGroupHandlesKHR(m_pipeline, 0, handleCount, dataSize, handles.data()) !=
        vk::Result::eSuccess) {
        throw std::runtime_error("failed to get ray tracing shader group handles!");
    }

    // allocate a butter for holding the handle data
    vk::DeviceSize sbtSize = m_rgenRegion.size + m_missRegion.size + m_hitRegion.size + m_callRegion.size;
    m_sbtBuffer            = m_pResourceManager->createBuffer(
        sbtSize,
        vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eShaderDeviceAddress |
            vk::BufferUsageFlagBits::eShaderBindingTableKHR,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    // find the SBT addresses of each group
    vk::DeviceAddress sbtAddress = m_pContext->getBufferDeviceAddress(m_sbtBuffer.descriptorInfo.buffer);
    m_rgenRegion.deviceAddress   = sbtAddress;
    m_missRegion.deviceAddress   = sbtAddress + m_rgenRegion.size;
    m_hitRegion.deviceAddress    = sbtAddress + m_rgenRegion.size + m_missRegion.size;

    // a helper to retrieve the handle data
    auto getHandle = [&](int i) { return handles.data() + i * handleSize; };

    // map the SBT buffer and write in the handles

    void *data = m_pContext->m_device.mapMemory(m_sbtBuffer.memory, 0, sbtSize);
    // memcpy(data, m_sbtBuffer, (size_t)sbtSize);
    uint8_t *pSbtBuffer = reinterpret_cast<uint8_t *>(data);
    uint8_t *pData      = nullptr;
    uint32_t handleIdx  = 0;

    // copy the raygen handle
    pData = pSbtBuffer;
    memcpy(pData, getHandle(handleIdx++), handleSize);

    // copy the miss group handles
    pData = pSbtBuffer + m_rgenRegion.size;
    for (uint32_t c = 0; c < missCount; ++c) {
        memcpy(pData, getHandle(handleIdx++), handleSize);
        pData += m_missRegion.stride;
    }

    // copy the hit group handles
    pData = pSbtBuffer + m_rgenRegion.size + m_missRegion.size;
    for (uint32_t c = 0; c < hitCount; ++c) {
        memcpy(pData, getHandle(handleIdx++), handleSize);
        pData += m_hitRegion.stride;
    }

    m_pContext->m_device.unmapMemory(m_sbtBuffer.memory);
}

void RayTracingRenderPass::setupRayTracingPipeline(const std::string &raygenShaderPath,
                                                   const std::string &missShaderPath,
                                                   const std::string &closestHitShaderPath) {
    m_raygenShaderPath     = raygenShaderPath;
    m_missShaderPath       = missShaderPath;
    m_closestHitShaderPath = closestHitShaderPath;

    enum StageIndices { eRaygen, eMiss, eClosestHit, eShaderGroupCount };

    auto raygenShaderCode     = readFile(m_raygenShaderPath);
    auto missShaderCode       = readFile(m_missShaderPath);
    auto closestHitShaderCode = readFile(m_closestHitShaderPath);

    std::array<vk::PipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
    vk::PipelineShaderStageCreateInfo stage{ .pName = "main" };

    // raygen
    stage.module    = createShaderModule(raygenShaderCode);
    stage.stage     = vk::ShaderStageFlagBits::eRaygenKHR;
    stages[eRaygen] = stage;

    // miss
    stage.module  = createShaderModule(missShaderCode);
    stage.stage   = vk::ShaderStageFlagBits::eMissKHR;
    stages[eMiss] = stage;

    // hit group - closest hit
    stage.module        = createShaderModule(closestHitShaderCode);
    stage.stage         = vk::ShaderStageFlagBits::eClosestHitKHR;
    stages[eClosestHit] = stage;

    // shader groups
    vk::RayTracingShaderGroupCreateInfoKHR group{ .generalShader      = VK_SHADER_UNUSED_KHR,
                                                  .closestHitShader   = VK_SHADER_UNUSED_KHR,
                                                  .anyHitShader       = VK_SHADER_UNUSED_KHR,
                                                  .intersectionShader = VK_SHADER_UNUSED_KHR };

    // raygen
    group.type          = vk::RayTracingShaderGroupTypeKHR::eGeneral;
    group.generalShader = eRaygen;
    m_shaderGroups.push_back(group);

    // miss
    group.type          = vk::RayTracingShaderGroupTypeKHR::eGeneral;
    group.generalShader = eMiss;
    m_shaderGroups.push_back(group);

    // closest hit
    group.type             = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
    group.generalShader    = VK_SHADER_UNUSED_KHR;
    group.closestHitShader = eClosestHit;
    m_shaderGroups.push_back(group);

    // setup the pipeline layout that will describe how the pipeline will access external data

    // define the push constant range used by the pipeline layout

    vk::PushConstantRange pushConstantRange{ .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR |
                                                           vk::ShaderStageFlagBits::eClosestHitKHR |
                                                           vk::ShaderStageFlagBits::eMissKHR,
                                             .offset = 0,
                                             .size   = sizeof(PushConstantRay) };

    vk::PipelineLayoutCreateInfo layoutCreateInfo{ .setLayoutCount         = 1,
                                                   .pSetLayouts            = &m_descriptorSetLayout,
                                                   .pushConstantRangeCount = 1,
                                                   .pPushConstantRanges    = &pushConstantRange };

    if (m_pContext->m_device.createPipelineLayout(&layoutCreateInfo, nullptr, &m_pipelineLayout) !=
        vk::Result::eSuccess) {
        throw std::runtime_error("failed to create a pipeline layout!");
    }

    // ray tracing pipeline can contain an arbitrary number of stages
    // depending on the number of active shaders in the scene.

    // assemble the shader stages and recursion depth info
    vk::RayTracingPipelineCreateInfoKHR rtPipelineInfo{ .stageCount = static_cast<uint32_t>(stages.size()),
                                                        .pStages    = stages.data(),
                                                        .groupCount = static_cast<uint32_t>(m_shaderGroups.size()),
                                                        .pGroups    = m_shaderGroups.data(),
                                                        .maxPipelineRayRecursionDepth = 1,
                                                        .layout                       = m_pipelineLayout };

    if (m_pContext->m_device.createRayTracingPipelinesKHR({}, {}, 1, &rtPipelineInfo, nullptr, &m_pipeline) !=
        vk::Result::eSuccess) {
        throw std::runtime_error("failed to create ray tracing pipelines");
    }

    for (auto &stage: stages) {
        m_pContext->m_device.destroyShaderModule(stage.module, nullptr);
    }
}

}; // namespace vuren