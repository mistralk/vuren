#include "Pipeline.hpp"

namespace vuren {

// RasterizationPipeline

RasterizationPipeline::RasterizationPipeline(VulkanContext* pContext, vk::DescriptorSetLayout descriptorSetLayout, RasterProperties rasterProperties)
    : Pipeline(pContext, descriptorSetLayout), m_rasterProperties(rasterProperties) {
}

void RasterizationPipeline::setup() {
    if (!m_pContext->m_device || !m_descriptorSetLayout) {
        throw std::runtime_error("pipeline setup failed! "
            "logical device and descriptor set layout must be valid before the pipeline creation.");
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

    auto vertShaderCode = readFile(m_rasterProperties.vertShaderPath);
    auto fragShaderCode = readFile(m_rasterProperties.fragShaderPath);

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
    if (m_rasterProperties.isBiltPass) {
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
        .cullMode = m_rasterProperties.isBiltPass ? vk::CullModeFlagBits::eNone : vk::CullModeFlagBits::eBack,
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

    std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments(m_rasterProperties.colorAttachmentCount, colorBlendAttachment);

    vk::PipelineColorBlendStateCreateInfo colorBlending { 
        .logicOpEnable = VK_FALSE,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = m_rasterProperties.colorAttachmentCount,
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
        .renderPass = m_rasterProperties.renderPass,
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

// RayTracingPipeline

RayTracingPipeline::RayTracingPipeline(VulkanContext* pContext, vk::DescriptorSetLayout descriptorSetLayout, RayTracingProperties rayTracingProperties)
    : Pipeline(pContext, descriptorSetLayout), m_rayTracingProperties(rayTracingProperties) {
}

void RayTracingPipeline::setup() {
    enum StageIndices {
        eRaygen,
        eMiss,
        eClosestHit,
        eShaderGroupCount
    };

    auto raygenShaderCode = readFile(m_rayTracingProperties.raygenShaderPath);
    auto missShaderCode = readFile(m_rayTracingProperties.missShaderPath);
    auto closestHitShaderCode = readFile(m_rayTracingProperties.closestHitShaderPath);

    std::array<vk::PipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
    vk::PipelineShaderStageCreateInfo stage { .pName = "main" };

    // raygen
    stage.module = createShaderModule(raygenShaderCode);
    stage.stage = vk::ShaderStageFlagBits::eRaygenKHR;
    stages[eRaygen] = stage;

    // miss
    stage.module = createShaderModule(missShaderCode);
    stage.stage = vk::ShaderStageFlagBits::eMissKHR;
    stages[eMiss] = stage;

    // hit group - closest hit
    stage.module = createShaderModule(closestHitShaderCode);
    stage.stage = vk::ShaderStageFlagBits::eClosestHitKHR;
    stages[eClosestHit] = stage;

    // shader groups
    vk::RayTracingShaderGroupCreateInfoKHR group {
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    };

    // raygen
    group.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
    group.generalShader = eRaygen;
    m_shaderGroups.push_back(group);

    // miss
    group.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
    group.generalShader = eMiss;
    m_shaderGroups.push_back(group);

    // closest hit
    group.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
    group.generalShader = VK_SHADER_UNUSED_KHR;
    group.closestHitShader = eClosestHit;
    m_shaderGroups.push_back(group);

    // setup the pipeline layout that will describe how the pipeline will access external data

    // define the push constant range used by the pipeline layout

    vk::PushConstantRange pushConstantRange {
        .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR,
        .offset = 0,
        .size = sizeof(PushConstantRay)};

    vk::PipelineLayoutCreateInfo layoutCreateInfo {
        .setLayoutCount = 1,
        .pSetLayouts = &m_descriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    if (m_pContext->m_device.createPipelineLayout(&layoutCreateInfo, nullptr, &m_pipelineLayout) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create a pipeline layout!");
    }

    // ray tracing pipeline can contain an arbitrary number of stages
    // depending on the number of active shaders in the scene.
    
    // assemble the shader stages and recursion depth info
    vk::RayTracingPipelineCreateInfoKHR rtPipelineInfo {
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .groupCount = static_cast<uint32_t>(m_shaderGroups.size()),
        .pGroups = m_shaderGroups.data(),
        .maxPipelineRayRecursionDepth = 1,
        .layout = m_pipelineLayout
    };

    if (m_pContext->m_device.createRayTracingPipelinesKHR({}, {}, 1, &rtPipelineInfo, nullptr, &m_pipeline) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create ray tracing pipelines");
    }

    for (auto& stage : stages) {
        m_pContext->m_device.destroyShaderModule(stage.module, nullptr);
    }
}

} // namespace vuren