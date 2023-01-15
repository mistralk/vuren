#include "RenderPass.hpp"
#include "VulkanContext.hpp"

namespace vrb {

void RenderPass::createVkRenderPass(const std::vector<AttachmentInfo>& colorAttachmentInfos, const AttachmentInfo& depthStencilAttachmentInfo) {
    if (m_rasterProperties.renderPass) {
        return;
    }

    std::vector<vk::AttachmentDescription> attachments; 
    std::vector<vk::AttachmentReference> colorAttachmentRefs; 
    vk::PipelineStageFlags srcStageMask = vk::PipelineStageFlagBits::eNone;
    vk::PipelineStageFlags dstStageMask = vk::PipelineStageFlagBits::eNone;
    vk::AccessFlags srcAccessMask = vk::AccessFlagBits::eNone;
    vk::AccessFlags dstAccessMask = vk::AccessFlagBits::eNone;

    // color attachments
    for (const auto& attachmentInfo : colorAttachmentInfos) {
        vk::AttachmentDescription colorAttachment { 
            .format = attachmentInfo.format, 
            .samples = vk::SampleCountFlagBits::e1,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = attachmentInfo.oldLayout,
            .finalLayout = attachmentInfo.newLayout
        };
        
        vk::AttachmentReference colorAttachmentRef { 
            .attachment = static_cast<uint32_t>(attachments.size()),
            .layout = attachmentInfo.newLayout == vk::ImageLayout::ePresentSrcKHR ? vk::ImageLayout::eColorAttachmentOptimal : attachmentInfo.newLayout
        };

        srcStageMask |= attachmentInfo.srcStageMask;
        dstStageMask |= attachmentInfo.dstStageMask;
        srcAccessMask |= attachmentInfo.srcAccessMask;
        dstAccessMask |= attachmentInfo.dstAccessMask;

        attachments.push_back(colorAttachment);
        colorAttachmentRefs.push_back(colorAttachmentRef);
    }
    
    // depth stencil attachment
    vk::AttachmentDescription depthAttachment { 
        .format = depthStencilAttachmentInfo.format,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eDontCare,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = depthStencilAttachmentInfo.oldLayout,
        .finalLayout = depthStencilAttachmentInfo.newLayout
    };

    vk::AttachmentReference depthAttachmentRef { 
        .attachment = static_cast<uint32_t>(attachments.size()),
        .layout = depthStencilAttachmentInfo.newLayout
    };
    
    attachments.push_back(depthAttachment);

    vk::SubpassDescription subpass = { 
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size()),
        .pColorAttachments = colorAttachmentRefs.data(),
        .pDepthStencilAttachment = &depthAttachmentRef 
    };
    
    // vk::SubpassDependency dependency { 
    //     .srcSubpass = VK_SUBPASS_EXTERNAL,
    //     .dstSubpass = 0,
    //     .srcStageMask = srcStageMask,
    //     .dstStageMask = dstStageMask,
    //     .srcAccessMask = srcAccessMask,
    //     .dstAccessMask = dstAccessMask
    // };

    vk::SubpassDependency dependency { 
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eFragmentShader,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits::eShaderRead,
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite
    };

    vk::RenderPassCreateInfo renderPassInfo { 
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency 
    };
    
    if (m_pContext->m_device.createRenderPass(&renderPassInfo, nullptr, &m_rasterProperties.renderPass) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void RenderPass::cleanup() {
    if (m_rasterProperties.framebuffer) m_pContext->m_device.destroyFramebuffer(m_rasterProperties.framebuffer, nullptr);
    if (m_rasterProperties.renderPass) m_pContext->m_device.destroyRenderPass(m_rasterProperties.renderPass, nullptr);
    if (m_descriptorPool) m_pContext->m_device.destroyDescriptorPool(m_descriptorPool, nullptr);
    if (m_descriptorSetLayout) m_pContext->m_device.destroyDescriptorSetLayout(m_descriptorSetLayout, nullptr);
    m_pPipeline->cleanup();
}

void RenderPass::createFramebuffer(const std::vector<AttachmentInfo>& colorAttachmentInfos, const AttachmentInfo& depthStencilAttachmentInfo) {
    std::vector<vk::ImageView> attachments;
    for (const auto& attachment : colorAttachmentInfos) {
        attachments.push_back(attachment.imageView);
    }
    attachments.push_back(depthStencilAttachmentInfo.imageView);

    vk::FramebufferCreateInfo framebufferInfo { 
        .renderPass = m_rasterProperties.renderPass,
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .width = m_extent.width,
        .height = m_extent.height,
        .layers = 1
    };

    if (m_pContext->m_device.createFramebuffer(&framebufferInfo, nullptr, &m_rasterProperties.framebuffer) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create framebuffer!");
    }

    m_rasterProperties.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentInfos.size());
}

void RenderPass::createDescriptorSet(const std::vector<ResourceBindingInfo>& bindingInfos) {
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    uint32_t totalDescriptorCount = 0;

    // create a descriptor set
    for (const auto& binding : bindingInfos) {
        vk::DescriptorSetLayoutBinding layoutBinding { 
            .binding = static_cast<uint32_t>(bindings.size()),
            .descriptorType = binding.descriptorType,
            .descriptorCount = binding.descriptorCount,
            .stageFlags = binding.stageFlags,
            .pImmutableSamplers = nullptr
        };

        bindings.push_back(layoutBinding);

        totalDescriptorCount += binding.descriptorCount;
    }

    vk::DescriptorSetLayoutCreateInfo layoutInfo { 
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };
    
    if (m_pContext->m_device.createDescriptorSetLayout(&layoutInfo, nullptr, &m_descriptorSetLayout) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create a descriptor set layout!");
    }

    // create a descriptor pool
    std::vector<vk::DescriptorPoolSize> poolSizes(bindings.size());
    for (size_t i = 0; i < poolSizes.size(); ++i) {
        poolSizes[i].type = bindings[i].descriptorType;
        poolSizes[i].descriptorCount = bindings[i].descriptorCount;
    }        
    
    vk::DescriptorPoolCreateInfo poolInfo { 
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    if (m_pContext->m_device.createDescriptorPool(&poolInfo, nullptr, &m_descriptorPool) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create a descriptor pool!");
    }

    // write a descriptor set
    vk::DescriptorSetAllocateInfo allocInfo { 
        .descriptorPool = m_descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_descriptorSetLayout
    };
    
    if (m_pContext->m_device.allocateDescriptorSets(&allocInfo, &m_descriptorSet) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to allocate offscreen descriptor sets!");
    }

    std::vector<vk::WriteDescriptorSet> descriptorWrites;
    std::vector<vk::DescriptorBufferInfo> bufferInfos;
    std::vector<vk::DescriptorImageInfo> imageInfos;

    bufferInfos.reserve(totalDescriptorCount);
    imageInfos.reserve(totalDescriptorCount);

    for (size_t i = 0; i < bindings.size(); ++i) {
        vk::WriteDescriptorSet write;
        vk::DescriptorBufferInfo bufferInfo;
        vk::DescriptorImageInfo imageInfo;

        write.pImageInfo = nullptr;
        write.pBufferInfo = nullptr;

        if (bindings[i].descriptorType == vk::DescriptorType::eAccelerationStructureKHR) {
            vk::AccelerationStructureKHR tlas = m_rayTracingProperties.tlas.as;
            vk::WriteDescriptorSetAccelerationStructureKHR descriptorSetAsInfo {
                .accelerationStructureCount = 1,
                .pAccelerationStructures = &tlas
            };
            
            write.pNext = (void*)&descriptorSetAsInfo;
            write.dstSet = m_descriptorSet;
            write.dstBinding = static_cast<uint32_t>(i);
            write.dstArrayElement = 0;
            write.descriptorCount = 1;
            write.descriptorType = bindings[i].descriptorType;
            write.pTexelBufferView = nullptr;
        }

        else {
            switch (bindings[i].descriptorType) {
                case vk::DescriptorType::eCombinedImageSampler:
                    if (bindingInfos[i].name == "SceneTextures") {
                        for (auto& texture : m_pScene->getTextures()) {
                            imageInfo = m_pResourceManager->getTexture(texture.name).descriptorInfo;
                            imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                            imageInfos.push_back(imageInfo);
                        }
                        write.pImageInfo = &imageInfos.back() - m_pScene->getTextures().size() + 1;
                    }
                    else {
                        imageInfo = m_pResourceManager->getTexture(bindingInfos[i].name).descriptorInfo;
                        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                        imageInfos.push_back(imageInfo);
                        write.pImageInfo = &imageInfos.back();
                    }
                    break;
                
                case vk::DescriptorType::eStorageImage:
                    imageInfo = m_pResourceManager->getTexture(bindingInfos[i].name).descriptorInfo;
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
                    if (bindingInfos[i].name == "SceneObjects") {
                        for (auto& buffer : m_pScene->getObjects()) {
                            bufferInfo = m_pResourceManager->getBuffer("SceneObjectDeviceInfo").descriptorInfo;
                            bufferInfos.push_back(bufferInfo);
                        }
                        write.pBufferInfo = &bufferInfos.back() - m_pScene->getObjects().size() + 1;
                    }
                    else {
                        bufferInfo = m_pResourceManager->getBuffer(bindingInfos[i].name).descriptorInfo;
                        bufferInfos.push_back(bufferInfo);
                        write.pBufferInfo = &bufferInfos.back();
                    }
                    break;

                default:
                    throw std::runtime_error("unsupported descriptor type!"); 
            }

            write.dstSet = m_descriptorSet;
            write.dstBinding = static_cast<uint32_t>(i);
            write.dstArrayElement = 0;
            write.descriptorCount = bindings[i].descriptorCount;
            write.descriptorType = bindings[i].descriptorType;
            write.pTexelBufferView = nullptr;
        }

        descriptorWrites.push_back(write);
    }
    
    m_pContext->m_device.updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

RenderPass::BlasInput RenderPass::objectToVkGeometryKHR(const SceneObject& object) {    
    // only the position attribute is needed for the AS build.
    // if position is not the first member of Vertex,
    // we have to manually adjust vertexAddress using offsetof.
    vk::DeviceAddress vertexAddress = m_pContext->getBufferDeviceAddress(object.vertexBuffer->descriptorInfo.buffer);
    vk::DeviceAddress indexAddress = m_pContext->getBufferDeviceAddress(object.indexBuffer->descriptorInfo.buffer);

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

// generate one BLAS for each BlasInput
void RenderPass::buildBlas(const std::vector<BlasInput>& input, vk::BuildAccelerationStructureFlagsKHR flags) {

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
        
        m_pContext->m_device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, &buildAs[i].buildInfo, maxPrimCount.data(), &buildAs[i].sizeInfo);

        asTotalSize += buildAs[i].sizeInfo.accelerationStructureSize;
        maxScratchSize = std::max(maxScratchSize, buildAs[i].sizeInfo.buildScratchSize);
        if ((buildAs[i].buildInfo.flags & vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction) == vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction)
            compactionsSize += 1;
    }

    // allocate the "largest" scratch buffer holding the temporary data of the acceleration structure builder
    Buffer scratchBuffer = m_pResourceManager->createBuffer(maxScratchSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);
    vk::DeviceAddress scratchAddress = m_pContext->getBufferDeviceAddress(scratchBuffer.descriptorInfo.buffer);

    vk::QueryPool queryPool{VK_NULL_HANDLE};

    if (compactionsSize > 0) {
        assert(compactionsSize == blasCount);
        vk::QueryPoolCreateInfo poolCreateInfo = {
            .queryType = vk::QueryType::eAccelerationStructureCompactedSizeKHR,
            .queryCount = blasCount
        };
        if (m_pContext->m_device.createQueryPool(&poolCreateInfo, nullptr, &queryPool) != vk::Result::eSuccess) {
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
            vk::CommandBuffer commandBuffer = beginSingleTimeCommands(*m_pContext, m_commandPool);

            // create BLAS
            if (queryPool)
                m_pContext->m_device.resetQueryPool(queryPool, 0, static_cast<uint32_t>(indices.size()));
            uint32_t queryCount = 0;

            for (const auto& j : indices) {
                vk::AccelerationStructureCreateInfoKHR createInfo {
                    .size = buildAs[j].sizeInfo.accelerationStructureSize,
                    .type = vk::AccelerationStructureTypeKHR::eBottomLevel
                };

                AccelerationStructure as;
                m_pResourceManager->createAs(createInfo, as);
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
            endSingleTimeCommands(*m_pContext, m_commandPool, commandBuffer);

            if (queryPool) {
                vk::CommandBuffer commandBuffer = beginSingleTimeCommands(*m_pContext, m_commandPool);

                // compact BLAS

                uint32_t queryCount = 0;
                std::vector<AccelerationStructure> cleanupAs; // previous AS to destroy

                // get the compacted size result back
                std::vector<vk::DeviceSize> compactSizes(static_cast<uint32_t>(indices.size()));
                if (m_pContext->m_device.getQueryPoolResults(queryPool, 0, (uint32_t)compactSizes.size(), compactSizes.size() * sizeof(vk::DeviceSize), compactSizes.data(), sizeof(vk::DeviceSize), vk::QueryResultFlagBits::eWait) != vk::Result::eSuccess) {
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
                    m_pResourceManager->createAs(asCreateInfo, buildAs[idx].as);

                    // copy the original BLAS to a compact version
                    vk::CopyAccelerationStructureInfoKHR copyInfo {
                        .src = buildAs[idx].buildInfo.dstAccelerationStructure,
                        .dst = buildAs[idx].as.as,
                        .mode = vk::CopyAccelerationStructureModeKHR::eCompact  
                    };
                    commandBuffer.copyAccelerationStructureKHR(&copyInfo);
                }
                
                // submit and wait
                endSingleTimeCommands(*m_pContext, m_commandPool, commandBuffer);

                // destroyNonCompacted
                for (auto& i : indices) {
                    m_pResourceManager->destroyAs(buildAs[i].cleanupAs);
                }
            }

            batchSize = 0;
            indices.clear();
        }
    }

    for (auto& b : buildAs) {
        m_rayTracingProperties.blas.emplace_back(b.as);
    }

    m_pContext->m_device.destroyQueryPool(queryPool, nullptr);
    m_pResourceManager->destroyBuffer(scratchBuffer);
}

void RenderPass::createBlas() {
    // BLAS stores each primitive in a geometry.
    std::vector<BlasInput> allBlas;
    allBlas.reserve(m_pScene->getObjects().size());

    for (const auto& obj : m_pScene->getObjects()) {
        auto blas = objectToVkGeometryKHR(obj);
        allBlas.emplace_back(blas);
    }

    buildBlas(allBlas, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
}

void RenderPass::buildTlas(const std::vector<vk::AccelerationStructureInstanceKHR>& instances, vk::BuildAccelerationStructureFlagsKHR flags, bool update) {
    assert(m_rayTracingProperties.tlas.as == vk::AccelerationStructureKHR{VK_NULL_HANDLE} || update);
    uint32_t instanceCount = static_cast<uint32_t>(instances.size());

    vk::CommandBuffer commandBuffer = beginSingleTimeCommands(*m_pContext, m_commandPool);;

    // caution: this is not the managed "InstanceBuffer"
    Buffer instanceBuffer = m_pResourceManager->createBufferByHostData<vk::AccelerationStructureInstanceKHR>(instances, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR, vk::MemoryPropertyFlagBits::eDeviceLocal);
    vk::DeviceAddress instanceBufferAddress = m_pContext->getBufferDeviceAddress(instanceBuffer.descriptorInfo.buffer);

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
    m_pContext->m_device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, &buildInfo, &instanceCount, &sizeInfo);

    vk::AccelerationStructureCreateInfoKHR createInfo {
        .size = sizeInfo.accelerationStructureSize,
        .type = vk::AccelerationStructureTypeKHR::eTopLevel
    };

    // create TLAS
    m_pResourceManager->createAs(createInfo, m_rayTracingProperties.tlas);

    // allocate the scratch memory
    Buffer scratchBuffer = m_pResourceManager->createBuffer(sizeInfo.buildScratchSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);
    vk::DeviceAddress scratchAddress = m_pContext->getBufferDeviceAddress(scratchBuffer.descriptorInfo.buffer);

    // update build information
    buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
    buildInfo.dstAccelerationStructure = m_rayTracingProperties.tlas.as;
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

    endSingleTimeCommands(*m_pContext, m_commandPool, commandBuffer);
    m_pResourceManager->destroyBuffer(scratchBuffer);
    m_pResourceManager->destroyBuffer(instanceBuffer);
}

void RenderPass::createTlas(const std::vector<ObjectInstance>& instances) {
    // TLAS is the entry point in the rt scene description
    std::vector<vk::AccelerationStructureInstanceKHR> tlas;
    tlas.reserve(instances.size());
    
    for (const ObjectInstance& instance : instances) {
        // glm transform: column-major matrix
        // VkTransform: row-major matrix
        // we need to transpose it.
        vk::TransformMatrixKHR transform = {
            .matrix = { {
                instance.world[0].x, instance.world[1].x, instance.world[2].x, instance.world[3].x,
                instance.world[0].y, instance.world[1].y, instance.world[2].y, instance.world[3].y,
                instance.world[0].z, instance.world[1].z, instance.world[2].z, instance.world[3].z
            } }
        };

        vk::DeviceAddress blasAddress;
        vk::AccelerationStructureDeviceAddressInfoKHR addressInfo = {
            .accelerationStructure = m_rayTracingProperties.blas[instance.objectId].as
        };
        blasAddress = m_pContext->m_device.getAccelerationStructureAddressKHR(addressInfo);
        
        vk::AccelerationStructureInstanceKHR rayInstance {
            .transform = transform, 
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

uint32_t RenderPass::align_up(uint32_t size, uint32_t alignment) {
    return (size + (alignment - 1)) & ~(alignment - 1);
}

void RenderPass::createShaderBindingTable() {
    uint32_t missCount = 1;
    uint32_t hitCount = 1;
    uint32_t handleCount = 1 + missCount + hitCount;
    uint32_t handleSize = m_rtProperties.shaderGroupHandleSize;
    
    // the SBT need to have starting groups to be aligned and handles in the group to be aligned
    uint32_t handleSizeAligned = align_up(handleSize, m_rtProperties.shaderGroupHandleAlignment);

    m_rgenRegion.stride = align_up(handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);
    m_rgenRegion.size = m_rgenRegion.stride; // pRayGenShaderBindingTable.size must be equal to its stride
    m_missRegion.stride = handleSizeAligned;
    m_missRegion.size = align_up(missCount * handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);
    m_hitRegion.stride = handleSizeAligned;
    m_hitRegion.size = align_up(hitCount * handleSizeAligned, m_rtProperties.shaderGroupBaseAlignment);

    // fetch the shader group handles of the pipeline
    uint32_t dataSize = handleCount * handleSize;
    std::vector<uint8_t> handles(dataSize);
    if (m_pContext->m_device.getRayTracingShaderGroupHandlesKHR(m_pPipeline->getPipeline(), 0, handleCount, dataSize, handles.data()) != vk::Result::eSuccess ) {
        throw std::runtime_error("failed to get ray tracing shader group handles!");
    }

    // allocate a butter for holding the handle data
    vk::DeviceSize sbtSize = m_rgenRegion.size + m_missRegion.size + m_hitRegion.size + m_callRegion.size;
    m_sbtBuffer = m_pResourceManager->createBuffer(sbtSize, 
                                vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eShaderBindingTableKHR,
                                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    
    // find the SBT addresses of each group
    vk::DeviceAddress sbtAddress = m_pContext->getBufferDeviceAddress(m_sbtBuffer.descriptorInfo.buffer);
    m_rgenRegion.deviceAddress = sbtAddress;
    m_missRegion.deviceAddress = sbtAddress + m_rgenRegion.size;
    m_hitRegion.deviceAddress = sbtAddress + m_rgenRegion.size + m_missRegion.size;

    // a helper to retrieve the handle data
    auto getHandle = [&] (int i) { return handles.data() + i * handleSize; };

    // map the SBT buffer and write in the handles

    void* data = m_pContext->m_device.mapMemory(m_sbtBuffer.memory, 0, sbtSize);
    // memcpy(data, m_sbtBuffer, (size_t)sbtSize);
    uint8_t* pSbtBuffer = reinterpret_cast<uint8_t*>(data);
    uint8_t* pData = nullptr;
    uint32_t handleIdx = 0;

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

void RenderPass::setupRasterPipeline(const std::string& vertShaderPath, const std::string& fragShaderPath, bool isBlitPass) {
    assert(m_pipelineType == PipelineType::eRasterization);

    m_rasterProperties.vertShaderPath = vertShaderPath;
    m_rasterProperties.fragShaderPath = fragShaderPath;
    m_rasterProperties.isBiltPass = isBlitPass;

    m_pPipeline = std::make_unique<RasterizationPipeline>(m_pContext, m_descriptorSetLayout, m_rasterProperties);

    m_pPipeline->setup();
}

void RenderPass::setupRayTracingPipeline(const std::string& raygenShaderPath, const std::string& missShaderPath, const std::string& closestHitShaderPath) {
    assert(m_pipelineType == PipelineType::eRayTracing);

    m_rayTracingProperties.raygenShaderPath = raygenShaderPath;
    m_rayTracingProperties.missShaderPath = missShaderPath;
    m_rayTracingProperties.closestHitShaderPath = closestHitShaderPath;

    m_pPipeline = std::make_unique<RayTracingPipeline>(m_pContext, m_descriptorSetLayout, m_rayTracingProperties);

    m_pPipeline->setup();
}

}; // namespace vrb