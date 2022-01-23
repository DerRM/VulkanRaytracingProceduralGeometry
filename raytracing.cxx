#include "raytracing.hxx"

#include <string.h>
#include <iostream>
#include <fstream>

CRayTracing::CRayTracing(VkInstance instance, VkDevice device, VkPhysicalDevice gpu, VkQueue queue, VkCommandPool commandPool, VkPhysicalDeviceRayTracingPipelinePropertiesKHR const& raytracingProperties)
    : m_instance(instance)
  , m_device(device)
    , m_gpu(gpu)
    , m_queue(queue)
    , m_commandPool(commandPool)
    , m_helper(CVulkanHelper(instance, device, gpu))
    , m_raytracingPipelineProperties(raytracingProperties)
{
}

void CRayTracing::init() {
    vkGetPhysicalDeviceMemoryProperties(m_gpu, &m_gpuMemProps);
}

void CRayTracing::initScene() {

    // Setup materials.
    {
        auto setAttributes = [&] (
            uint32_t primitiveIndex,
            glm::vec4 const& albedo,
            float reflectanceCoef = 0.0f,
            float diffuseCoef = 0.9f,
            float specularCoef = 0.7f,
            float specularPower = 50.0f,
            float stepScale = 1.0f)
        {
            PrimitiveConstantBuffer& attributes = m_aabbMaterialCB[primitiveIndex];
            attributes.albedo = albedo;
            attributes.reflectanceCoef = reflectanceCoef;
            attributes.diffuseCoef = diffuseCoef;
            attributes.specularCoef = specularCoef;
            attributes.specularPower = specularPower;
            attributes.stepScale = stepScale;
        };

        m_planeMaterialCB = { glm::vec4(0.9f, 0.9f, 0.9f, 1.0f), 0.25f, 1.0f, 0.4f, 50.0f, 1.0f, /*padding*/ glm::vec3(0.0f) };

        glm::vec4 green = glm::vec4(0.1f, 1.0f, 0.5f, 1.0f);
        glm::vec4 red = glm::vec4(1.0f, 0.5f, 0.5f, 1.0f);
        glm::vec4 yellow = glm::vec4(1.0f, 1.0f, 0.5f, 1.0f);

        uint32_t offset = 0;

        {
            setAttributes(offset + AnalyticPrimitive::AABB, red);
            setAttributes(offset + AnalyticPrimitive::Spheres, kChromiumReflectance, 1.0f);
            offset += AnalyticPrimitive::Count;
        }

        {
            setAttributes(offset + VolumetricPrimitive::Metaballs, kChromiumReflectance, 1.0f);
            offset += VolumetricPrimitive::Count;
        }

        {
            setAttributes(offset + SignedDistancePrimitive::MiniSpheres, green);
            setAttributes(offset + SignedDistancePrimitive::IntersectedRoundCube, green);
            setAttributes(offset + SignedDistancePrimitive::SquareTorus, kChromiumReflectance, 1.0f);
            setAttributes(offset + SignedDistancePrimitive::TwistedTorus, yellow, 0.0f, 1.0f, 0.7f, 50.0f, 0.5f);
            setAttributes(offset + SignedDistancePrimitive::Cog, yellow, 0.0f, 1.0f, 0.1f, 2.0f);
            setAttributes(offset + SignedDistancePrimitive::Cylinder, red);
            setAttributes(offset + SignedDistancePrimitive::FractalPyramid, green, 0.0f, 1.0f, 0.1f, 4.0f, 0.8f);
        }
    }

    // Setup camera.
    {
        m_eye = { 0.0f, 5.3f, -17.0f, 1.0f };
        m_at = { 0.0f, 0.0f, 0.0f, 1.0f };
        glm::vec4 right = { 1.0f, 0.0f, 0.0f, 0.0f };

        glm::vec4 direction = glm::normalize(m_at - m_eye);
        m_up = glm::vec4(glm::normalize(glm::cross(glm::vec3(direction), glm::vec3(right))), 0.0f);


        glm::mat4 rotate = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m_eye = rotate * m_eye;
        m_up = rotate * m_up;

        updateCameraMatrices();
    }

    // Setup lights.
    {
        glm::vec4 lightPosition;
        glm::vec4 lightAmbientColor;
        glm::vec4 lightDiffuseColor;

        lightPosition = glm::vec4(0.0f, 18.0f, -20.0f, 0.0f);
        m_sceneCB.lightPosition = lightPosition;

        lightAmbientColor = glm::vec4(0.25f, 0.25f, 0.25f, 1.0f);
        m_sceneCB.lightAmbientColor = lightAmbientColor;

        float d = 0.6f;
        lightDiffuseColor = glm::vec4(d, d, d, 1.0f);
        m_sceneCB.lightDiffuseColor = lightDiffuseColor;
    }

    uint32_t instanceIndex = 0;

    for (uint32_t primitiveIndex = 0; primitiveIndex < AnalyticPrimitive::Count; ++primitiveIndex) {
        m_aabbInstanceCB[instanceIndex].instanceIndex = instanceIndex;
        m_aabbInstanceCB[instanceIndex].primitiveType = primitiveIndex;
        ++instanceIndex;
    }

    for (uint32_t primitiveIndex = 0; primitiveIndex < VolumetricPrimitive::Count; ++primitiveIndex) {
        m_aabbInstanceCB[instanceIndex].instanceIndex = instanceIndex;
        m_aabbInstanceCB[instanceIndex].primitiveType = primitiveIndex;
        ++instanceIndex;
    }

    for (uint32_t primitiveIndex = 0; primitiveIndex < SignedDistancePrimitive::Count; ++primitiveIndex) {
        m_aabbInstanceCB[instanceIndex].instanceIndex = instanceIndex;
        m_aabbInstanceCB[instanceIndex].primitiveType = primitiveIndex;
        ++instanceIndex;
    }
}

void CRayTracing::createSceneBuffer() {

    m_sceneBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, sizeof(SceneConstantBuffer), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void CRayTracing::updateSceneBuffer() {
    m_helper.copyToBuffer(m_sceneBuffer, &m_sceneCB, sizeof(SceneConstantBuffer));
}

void CRayTracing::createAABBPrimitiveBuffer() {
    m_aabbPrimitiveBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, sizeof(PrimitiveInstancePerFrameBuffer) * IntersectionShaderType::kTotalPrimitiveCount, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void CRayTracing::updateAABBPrimitiveBuffer() {
    m_helper.copyToBuffer(m_aabbPrimitiveBuffer, m_aabbPrimitiveAttributeBuffer, sizeof(PrimitiveInstancePerFrameBuffer) * IntersectionShaderType::kTotalPrimitiveCount);
}

void CRayTracing::createShader(VkShaderStageFlagBits type, std::string const& shader_source) {

  uint8_t* memory = nullptr;

  std::streampos size = 0;
  std::ifstream file(shader_source, std::ios::in | std::ios::binary | std::ios::ate);
  if (file.is_open()) {
    size = file.tellg();
    memory = new uint8_t[size];
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(memory), size);
    file.close();
  }


    VkShaderModuleCreateInfo shaderInfo = {};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = static_cast<size_t>(size);
    shaderInfo.pCode = reinterpret_cast<uint32_t*>(memory);

    VkShaderModule shaderModule;
    VkResult res = vkCreateShaderModule(m_device, &shaderInfo, nullptr, &shaderModule);
    if (res != VK_SUCCESS) {
        printf("could not create shader module\n");
    }

  delete[] memory;

    VkPipelineShaderStageCreateInfo shaderStageInfo = {};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = type;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";

    m_shaderStages.push_back(shaderStageInfo);
}

VkPipeline CRayTracing::createPipeline(VkPipelineLayout pipelineLayout) {

    m_shaderGroups.clear();

    createRayGenShaderGroups();

    for (size_t index = 0; index < m_rayGenShaderGroups.size(); ++index) {
        m_shaderGroups.push_back(m_rayGenShaderGroups[index]);
    }

    createMissShaderGroups();

    for (size_t index = 0; index < m_missShaderGroups.size(); ++index) {
        m_shaderGroups.push_back(m_missShaderGroups[index]);
    }

    createHitShaderGroups();

    for (size_t index = 0; index < m_hitShaderGroups.size(); ++index) {
        m_shaderGroups.push_back(m_hitShaderGroups[index]);
    }

    VkRayTracingPipelineCreateInfoKHR raytracingPipelineInfo = {};
    raytracingPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    raytracingPipelineInfo.maxPipelineRayRecursionDepth = 3;
    raytracingPipelineInfo.stageCount = static_cast<uint32_t>(m_shaderStages.size());
    raytracingPipelineInfo.pStages = m_shaderStages.data();
    raytracingPipelineInfo.groupCount = static_cast<uint32_t>(m_shaderGroups.size());
    raytracingPipelineInfo.pGroups = m_shaderGroups.data();
    raytracingPipelineInfo.layout = pipelineLayout;
    raytracingPipelineInfo.basePipelineIndex = 0;
    raytracingPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    vkCreateRayTracingPipelinesKHR(m_device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &raytracingPipelineInfo, nullptr, &m_raytracingPipeline);

    createRayGenShaderTable();
    //createMissShaderTable();
    //createHitShaderTable();

    return m_raytracingPipeline;
}

VulkanBuffer CRayTracing::getRayGenShaderGroups() {
    return m_raygenShaderGroupBuffer;
}

VulkanBuffer CRayTracing::getMissShaderGroups() {
    return m_missShaderGroupBuffer;
}

VulkanBuffer CRayTracing::getHitShaderGroups() {
    return m_hitShaderGroupBuffer;
}

void CRayTracing::createRayGenShaderTable() {
    uint32_t raygenAlignment = CVulkanHelper::alignTo(m_raytracingPipelineProperties.shaderGroupHandleSize * m_rayGenShaderGroups.size(), m_raytracingPipelineProperties.shaderGroupBaseAlignment);
    uint32_t missAlignment = CVulkanHelper::alignTo(m_raytracingPipelineProperties.shaderGroupHandleSize * m_missShaderGroups.size(), m_raytracingPipelineProperties.shaderGroupBaseAlignment);

    VkDeviceSize bufferSize =
            raygenAlignment
            + missAlignment
            // we don't need to align the last part as only the base addresses must be aligned and not the buffer itself
            + (m_raytracingPipelineProperties.shaderGroupHandleSize + sizeof(PrimitiveConstantBuffer) + sizeof(PrimitiveInstanceConstantBuffer)) * m_aabbs.size();
    m_raygenShaderGroupBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, bufferSize, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data = nullptr;
    vkMapMemory(m_device, m_raygenShaderGroupBuffer.memory, 0, bufferSize, 0, &data);
    uint8_t* mappedMemory = (uint8_t*)data;
    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(m_device, m_raytracingPipeline, 0, static_cast<uint32_t>(m_rayGenShaderGroups.size()), m_raytracingPipelineProperties.shaderGroupHandleSize * m_rayGenShaderGroups.size(), mappedMemory));
    mappedMemory += raygenAlignment;
    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(m_device, m_raytracingPipeline, static_cast<uint32_t>(m_rayGenShaderGroups.size()), static_cast<uint32_t>(m_missShaderGroups.size()), m_raytracingPipelineProperties.shaderGroupHandleSize * m_missShaderGroups.size(), mappedMemory));
    mappedMemory += missAlignment;
    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(m_device, m_raytracingPipeline, static_cast<uint32_t>(m_rayGenShaderGroups.size() + m_missShaderGroups.size()), 1, m_raytracingPipelineProperties.shaderGroupHandleSize, mappedMemory));
    mappedMemory += m_raytracingPipelineProperties.shaderGroupHandleSize;
    memcpy(mappedMemory, &m_planeMaterialCB, sizeof(PrimitiveConstantBuffer));
    mappedMemory += sizeof(PrimitiveConstantBuffer) + sizeof(PrimitiveInstanceConstantBuffer);

    uint32_t offset = 0;

    for (size_t index = 0; index < AnalyticPrimitive::Count; ++index) {
        VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(m_device, m_raytracingPipeline, static_cast<uint32_t>(m_rayGenShaderGroups.size() + m_missShaderGroups.size() + 1), 1, m_raytracingPipelineProperties.shaderGroupHandleSize, mappedMemory));
        mappedMemory += m_raytracingPipelineProperties.shaderGroupHandleSize;
        memcpy(mappedMemory, &m_aabbMaterialCB[index + offset], sizeof(PrimitiveConstantBuffer));
        mappedMemory += sizeof(m_aabbMaterialCB[0]);
        memcpy(mappedMemory, &m_aabbInstanceCB[index + offset], sizeof(PrimitiveInstanceConstantBuffer));
        mappedMemory += sizeof(m_aabbInstanceCB[0]);
    }

    offset += AnalyticPrimitive::Count;

    for (size_t index = 0; index < VolumetricPrimitive::Count; ++index) {
        VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(m_device, m_raytracingPipeline, static_cast<uint32_t>(m_rayGenShaderGroups.size() + m_missShaderGroups.size() + 2), 1, m_raytracingPipelineProperties.shaderGroupHandleSize, mappedMemory));
        mappedMemory += m_raytracingPipelineProperties.shaderGroupHandleSize;
        memcpy(mappedMemory, &m_aabbMaterialCB[index + offset], sizeof(PrimitiveConstantBuffer));
        mappedMemory += sizeof(m_aabbMaterialCB[0]);
        memcpy(mappedMemory, &m_aabbInstanceCB[index + offset], sizeof(PrimitiveInstanceConstantBuffer));
        mappedMemory += sizeof(m_aabbInstanceCB[0]);
    }

    offset += VolumetricPrimitive::Count;

    for (size_t index = 0; index < SignedDistancePrimitive::Count; ++index) {
        VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(m_device, m_raytracingPipeline, static_cast<uint32_t>(m_rayGenShaderGroups.size() + m_missShaderGroups.size() + 3), 1, m_raytracingPipelineProperties.shaderGroupHandleSize, mappedMemory));
        mappedMemory += m_raytracingPipelineProperties.shaderGroupHandleSize;
        memcpy(mappedMemory, &m_aabbMaterialCB[index + offset], sizeof(PrimitiveConstantBuffer));
        mappedMemory += sizeof(m_aabbMaterialCB[0]);
        memcpy(mappedMemory, &m_aabbInstanceCB[index + offset], sizeof(PrimitiveInstanceConstantBuffer));
        mappedMemory += sizeof(m_aabbInstanceCB[0]);
    }

    vkUnmapMemory(m_device, m_raygenShaderGroupBuffer.memory);
}

void CRayTracing::createMissShaderTable() {
    VkDeviceSize bufferSize = m_raytracingPipelineProperties.shaderGroupHandleSize * m_missShaderGroups.size();
    m_missShaderGroupBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, bufferSize, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    void* data = nullptr;
    vkMapMemory(m_device, m_missShaderGroupBuffer.memory, 0, bufferSize, 0, &data);
    uint8_t* mappedMemory = (uint8_t*)data;
    vkGetRayTracingShaderGroupHandlesKHR(m_device, m_raytracingPipeline, static_cast<uint32_t>(m_rayGenShaderGroups.size()), static_cast<uint32_t>(m_missShaderGroups.size()), bufferSize, mappedMemory);
    vkUnmapMemory(m_device, m_missShaderGroupBuffer.memory);
}

void CRayTracing::createHitShaderTable() {
    VkDeviceSize bufferSize = (m_raytracingPipelineProperties.shaderGroupHandleSize + sizeof(PrimitiveConstantBuffer) + sizeof(PrimitiveInstanceConstantBuffer)) * m_hitShaderGroups.size();
    m_hitShaderGroupBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, bufferSize, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    void* data = nullptr;
    vkMapMemory(m_device, m_hitShaderGroupBuffer.memory, 0, bufferSize, 0, &data);
    uint8_t* mappedMemory = (uint8_t*)data;

    vkGetRayTracingShaderGroupHandlesKHR(m_device, m_raytracingPipeline, static_cast<uint32_t>(m_rayGenShaderGroups.size() + m_missShaderGroups.size()), m_raytracingPipelineProperties.shaderGroupHandleSize, bufferSize, mappedMemory);
    mappedMemory += m_raytracingPipelineProperties.shaderGroupHandleSize;
    memcpy(mappedMemory, &m_planeMaterialCB, sizeof(PrimitiveConstantBuffer));
    mappedMemory += sizeof(PrimitiveConstantBuffer) + sizeof(PrimitiveInstanceConstantBuffer);

    //for (size_t index = 1; index < m_hitShaderGroups.size(); ++index) {
        vkGetRayTracingShaderGroupHandlesKHR(m_device, m_raytracingPipeline, static_cast<uint32_t>(m_rayGenShaderGroups.size() + m_missShaderGroups.size() /*+ index*/ + 1), m_raytracingPipelineProperties.shaderGroupHandleSize, bufferSize, mappedMemory);
        mappedMemory += m_raytracingPipelineProperties.shaderGroupHandleSize;
        memcpy(mappedMemory, &m_aabbMaterialCB[/*index - 1*/2], sizeof(PrimitiveConstantBuffer));
        mappedMemory += sizeof(PrimitiveConstantBuffer);
        memcpy(mappedMemory, &m_aabbInstanceCB[/*index - 1*/2], sizeof(PrimitiveInstanceConstantBuffer));
        mappedMemory += sizeof(PrimitiveInstanceConstantBuffer);

    //    vkGetRayTracingShaderGroupHandles(m_device, m_raytracingPipeline, m_rayGenShaderGroups.size() + m_missShaderGroups.size() /*+ index*/ + 2, m_raytracingProperties.shaderGroupHandleSize, bufferSize, mappedMemory);
    //    mappedMemory += m_raytracingProperties.shaderGroupHandleSize;
    //    memcpy(mappedMemory, &m_aabbMaterialCB[/*index - 1*/2], sizeof(PrimitiveConstantBuffer));
    //    mappedMemory += sizeof(PrimitiveConstantBuffer);
    //    memcpy(mappedMemory, &m_aabbInstanceCB[/*index - 1*/2], sizeof(PrimitiveInstanceConstantBuffer));
    //    mappedMemory += sizeof(PrimitiveInstanceConstantBuffer);

    //}
    vkUnmapMemory(m_device, m_hitShaderGroupBuffer.memory);
}

VulkanImage CRayTracing::createOffscreenImage(VkFormat format, uint32_t width, uint32_t height) {
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = { width, height, 1 };
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.queueFamilyIndexCount = 0;
    imageInfo.pQueueFamilyIndices = nullptr;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage offscreenImage;
    vkCreateImage(m_device, &imageInfo, nullptr, &offscreenImage);

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(m_device, offscreenImage, &memoryRequirements);

    uint32_t memoryType = m_helper.getMemoryType(memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateInfo memoryAllocInfo = {};
    memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocInfo.allocationSize = memoryRequirements.size;
    memoryAllocInfo.memoryTypeIndex = memoryType;

    VkDeviceMemory offsreenImageMemory;
    vkAllocateMemory(m_device, &memoryAllocInfo, nullptr, &offsreenImageMemory);

    vkBindImageMemory(m_device, offscreenImage, offsreenImageMemory, 0);

    VulkanImage image;
    image.handle = offscreenImage;
    image.memory = offsreenImageMemory;
    image.size = memoryRequirements.size;
    image.format = format;
    image.width = width;
    image.height = height;

    m_offscreenImage = image;

    return image;
}

void CRayTracing::updateDescriptors(VkDescriptorSet descriptorSet) {

    VkImageViewCreateInfo offscreenImageViewInfo = {};
    offscreenImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    offscreenImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    offscreenImageViewInfo.format = m_offscreenImage.format;
    offscreenImageViewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    offscreenImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    offscreenImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    offscreenImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    offscreenImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    offscreenImageViewInfo.image = m_offscreenImage.handle;

    VkImageView offscreenImageView;
    vkCreateImageView(m_device, &offscreenImageViewInfo, nullptr, &offscreenImageView);

    VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo = {};
    descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    descriptorAccelerationStructureInfo.pAccelerationStructures = &m_topLevelAs;

    VkWriteDescriptorSet accelerationStructureWrite = {};
    accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
    accelerationStructureWrite.dstSet = descriptorSet;
    accelerationStructureWrite.descriptorCount = 1;
    accelerationStructureWrite.dstBinding = 0;
    accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    VkDescriptorImageInfo descriptorOutputImageInfo = {};
    descriptorOutputImageInfo.sampler = VK_NULL_HANDLE;
    descriptorOutputImageInfo.imageView = offscreenImageView;
    descriptorOutputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet outputImageWrite = {};
    outputImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    outputImageWrite.dstSet = descriptorSet;
    outputImageWrite.dstBinding = 1;
    outputImageWrite.dstArrayElement = 0;
    outputImageWrite.descriptorCount = 1;
    outputImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    outputImageWrite.pImageInfo = &descriptorOutputImageInfo;

    VkDescriptorBufferInfo descriptorSceneBufferInfo = {};
    descriptorSceneBufferInfo.buffer = m_sceneBuffer.handle;
    descriptorSceneBufferInfo.range = m_sceneBuffer.size;

    VkWriteDescriptorSet sceneBufferWrite = {};
    sceneBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sceneBufferWrite.dstSet = descriptorSet;
    sceneBufferWrite.dstBinding = 2;
    sceneBufferWrite.dstArrayElement = 0;
    sceneBufferWrite.descriptorCount = 1;
    sceneBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sceneBufferWrite.pBufferInfo = &descriptorSceneBufferInfo;

    VkDescriptorBufferInfo descriptorFacesBufferInfo = {};
    descriptorFacesBufferInfo.buffer = m_facesBuffer.handle;
    descriptorFacesBufferInfo.range = m_facesBuffer.size;

    VkWriteDescriptorSet facesBufferWrite = {};
    facesBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    facesBufferWrite.dstSet = descriptorSet;
    facesBufferWrite.dstBinding = 3;
    facesBufferWrite.dstArrayElement = 0;
    facesBufferWrite.descriptorCount = 1;
    facesBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    facesBufferWrite.pBufferInfo = &descriptorFacesBufferInfo;

    VkDescriptorBufferInfo descriptorNormalBufferInfo = {};
    descriptorNormalBufferInfo.buffer = m_normalBuffer.handle;
    descriptorNormalBufferInfo.range = m_normalBuffer.size;

    VkWriteDescriptorSet normalBufferWrite = {};
    normalBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    normalBufferWrite.dstSet = descriptorSet;
    normalBufferWrite.dstBinding = 4;
    normalBufferWrite.dstArrayElement = 0;
    normalBufferWrite.descriptorCount = 1;
    normalBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    normalBufferWrite.pBufferInfo = &descriptorNormalBufferInfo;

    VkDescriptorBufferInfo descriptorAABBPrimitiveBufferInfo = {};
    descriptorAABBPrimitiveBufferInfo.buffer = m_aabbPrimitiveBuffer.handle;
    descriptorAABBPrimitiveBufferInfo.range = m_aabbPrimitiveBuffer.size;

    VkWriteDescriptorSet sceneAABBPrimitiveBufferWrite = {};
    sceneAABBPrimitiveBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sceneAABBPrimitiveBufferWrite.dstSet = descriptorSet;
    sceneAABBPrimitiveBufferWrite.dstBinding = 5;
    sceneAABBPrimitiveBufferWrite.dstArrayElement = 0;
    sceneAABBPrimitiveBufferWrite.descriptorCount = 1;
    sceneAABBPrimitiveBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sceneAABBPrimitiveBufferWrite.pBufferInfo = &descriptorAABBPrimitiveBufferInfo;

    std::vector<VkWriteDescriptorSet> descriptorWrites({accelerationStructureWrite, outputImageWrite, sceneBufferWrite, facesBufferWrite, normalBufferWrite, sceneAABBPrimitiveBufferWrite});
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void CRayTracing::createPrimitives() {

}

void CRayTracing::createShaderStages() {
    //createShader(VK_SHADER_STAGE_RAYGEN_BIT_NV, "shader/raygen_nv.spv");
    //createShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, "shader/closest_hit_triangle_nv.spv");
    //createShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, "shader/closest_hit_aabb_nv.spv");
    //createShader(VK_SHADER_STAGE_MISS_BIT_NV, "shader/miss_nv.spv");
    //createShader(VK_SHADER_STAGE_MISS_BIT_NV, "shader/miss_shadow_ray_nv.spv");
    //createShader(VK_SHADER_STAGE_INTERSECTION_BIT_NV, "shader/intersection_analytic_nv.spv");
    //createShader(VK_SHADER_STAGE_INTERSECTION_BIT_NV, "shader/intersection_volumetric_nv.spv");
    //createShader(VK_SHADER_STAGE_INTERSECTION_BIT_NV, "shader/intersection_signed_distance_nv.spv");
    createShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, "shader/raygen_ext.spv");
    createShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "shader/closest_hit_triangle_ext.spv");
    createShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "shader/closest_hit_aabb_ext.spv");
    createShader(VK_SHADER_STAGE_MISS_BIT_KHR, "shader/miss_ext.spv");
    createShader(VK_SHADER_STAGE_MISS_BIT_KHR, "shader/miss_shadow_ray_ext.spv");
    createShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, "shader/intersection_analytic_ext.spv");
    createShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, "shader/intersection_volumetric_ext.spv");
    createShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, "shader/intersection_signed_distance_ext.spv");
}

void CRayTracing::createRayGenShaderGroups() {

    VkRayTracingShaderGroupCreateInfoKHR raygenShaderGroupInfo = {};
    raygenShaderGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    raygenShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    raygenShaderGroupInfo.generalShader = 0;
    raygenShaderGroupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
    raygenShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
    raygenShaderGroupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

    m_rayGenShaderGroups.push_back(raygenShaderGroupInfo);
}

void CRayTracing::createMissShaderGroups() {

    VkRayTracingShaderGroupCreateInfoKHR missShaderGroupInfo = {};
    missShaderGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    missShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    missShaderGroupInfo.generalShader = 3;
    missShaderGroupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
    missShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
    missShaderGroupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

    m_missShaderGroups.push_back(missShaderGroupInfo);


    missShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    missShaderGroupInfo.generalShader = 4;
    missShaderGroupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
    missShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
    missShaderGroupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

    m_missShaderGroups.push_back(missShaderGroupInfo);

}

void CRayTracing::createHitShaderGroups() {

    VkRayTracingShaderGroupCreateInfoKHR closestHitShaderGroupInfo = {};
    closestHitShaderGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    closestHitShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    closestHitShaderGroupInfo.generalShader = VK_SHADER_UNUSED_KHR;
    closestHitShaderGroupInfo.closestHitShader = 1;
    closestHitShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
    closestHitShaderGroupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

    m_hitShaderGroups.push_back(closestHitShaderGroupInfo);

    closestHitShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
    closestHitShaderGroupInfo.generalShader = VK_SHADER_UNUSED_KHR;
    closestHitShaderGroupInfo.closestHitShader = 2;
    closestHitShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
    closestHitShaderGroupInfo.intersectionShader = 5;

    m_hitShaderGroups.push_back(closestHitShaderGroupInfo);

    closestHitShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
    closestHitShaderGroupInfo.generalShader = VK_SHADER_UNUSED_KHR;
    closestHitShaderGroupInfo.closestHitShader = 2;
    closestHitShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
    closestHitShaderGroupInfo.intersectionShader = 6;

    m_hitShaderGroups.push_back(closestHitShaderGroupInfo);

    closestHitShaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
    closestHitShaderGroupInfo.generalShader = VK_SHADER_UNUSED_KHR;
    closestHitShaderGroupInfo.closestHitShader = 2;
    closestHitShaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
    closestHitShaderGroupInfo.intersectionShader = 7;

    m_hitShaderGroups.push_back(closestHitShaderGroupInfo);
}

void CRayTracing::createCommandBuffers() {

}

void CRayTracing::buildAccelerationStructurePlane() {


}

BottomLevelAccelerationStructure CRayTracing::createBottomLevelAccelerationStructure(VkAccelerationStructureBuildSizesInfoKHR const& asBuildSizes) {

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.size = asBuildSizes.accelerationStructureSize;

    VulkanBuffer accelerationBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, asBuildSizes.accelerationStructureSize, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkAccelerationStructureCreateInfoKHR accelerationStructureInfo = {};
    accelerationStructureInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    accelerationStructureInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    accelerationStructureInfo.buffer = accelerationBuffer.handle;
    accelerationStructureInfo.offset = 0;
    accelerationStructureInfo.size = asBuildSizes.accelerationStructureSize;

    VkAccelerationStructureKHR accelerationStructure;
    VK_CHECK(vkCreateAccelerationStructureKHR(m_device, &accelerationStructureInfo, nullptr, &accelerationStructure));

    VkAccelerationStructureDeviceAddressInfoKHR accDeviceAddressInfo = {};
    accDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    accDeviceAddressInfo.accelerationStructure = accelerationStructure;

    VkDeviceAddress accelerationStructureHandle = vkGetAccelerationStructureDeviceAddressKHR(m_device, &accDeviceAddressInfo);    

    BottomLevelAccelerationStructure accStruct = {};
    accStruct.buffer = accelerationBuffer;
    accStruct.handle = accelerationStructure;
    accStruct.gpuAddress = accelerationStructureHandle;
    return accStruct;
}

void CRayTracing::buildTriangleAccelerationStructure() {

    buildPlaneGeometry();

    VkAccelerationStructureGeometryKHR triangleGeometry = {};
    triangleGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    triangleGeometry.pNext = nullptr;
    triangleGeometry.flags = 0;
    triangleGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    triangleGeometry.geometry = {};
    triangleGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangleGeometry.geometry.triangles.pNext = nullptr;
    triangleGeometry.geometry.triangles.vertexData.deviceAddress = m_vertexBuffer.address;
    triangleGeometry.geometry.triangles.vertexStride = sizeof(Vertex);
    triangleGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangleGeometry.geometry.triangles.indexData.deviceAddress = m_indexBuffer.address;
    triangleGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT16;
    triangleGeometry.geometry.triangles.maxVertex = 4;

    VkAccelerationStructureBuildGeometryInfoKHR triangleGeometryInfo = {};
    triangleGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    triangleGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    triangleGeometryInfo.geometryCount = 1;
    triangleGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    triangleGeometryInfo.pGeometries = &triangleGeometry;

    VkAccelerationStructureBuildSizesInfoKHR triangleAccelerationStructureSizes;
    triangleAccelerationStructureSizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(m_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &triangleGeometryInfo, &triangleGeometryInfo.geometryCount, &triangleAccelerationStructureSizes);

    BottomLevelAccelerationStructure triangleAccStruct = createBottomLevelAccelerationStructure(triangleAccelerationStructureSizes);

    std::vector<VkAccelerationStructureGeometryKHR> aabbGeometries(m_aabbBuffers.size());
    std::vector<VkAccelerationStructureBuildSizesInfoKHR> aabbAsBuildSizes(m_aabbBuffers.size());
    std::vector<BottomLevelAccelerationStructure> accStructs(m_aabbBuffers.size());
    
    for (size_t index = 0; index < m_aabbBuffers.size(); ++index) {
        aabbGeometries[index] = {};
        aabbGeometries[index].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        aabbGeometries[index].pNext = nullptr;
        aabbGeometries[index].geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
        aabbGeometries[index].geometry = {};
        aabbGeometries[index].geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
        aabbGeometries[index].geometry.aabbs.stride = sizeof(VkAabbPositionsKHR);
        aabbGeometries[index].geometry.aabbs.data.deviceAddress = m_aabbBuffers[index].address;
        aabbGeometries[index].flags = 0;

        VkAccelerationStructureBuildGeometryInfoKHR aabbGeometryInfo = {};
        aabbGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        aabbGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        aabbGeometryInfo.geometryCount = 1;
        aabbGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        aabbGeometryInfo.pGeometries = &aabbGeometries[index];

        aabbAsBuildSizes[index] = {};
        aabbAsBuildSizes[index].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(m_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &aabbGeometryInfo, &aabbGeometryInfo.geometryCount, &aabbAsBuildSizes[index]);

        accStructs[index] = createBottomLevelAccelerationStructure(aabbAsBuildSizes[index]);
    }

    glm::uvec3 const kNumAabb = glm::uvec3(700, 1, 700);
    glm::vec3 const vWidth = glm::vec3(
        kNumAabb.x * kAabbWidth + (kNumAabb.x - 1) * kAabbDistance,
        kNumAabb.y * kAabbWidth + (kNumAabb.y - 1) * kAabbDistance,
        kNumAabb.z * kAabbWidth + (kNumAabb.z - 1) * kAabbDistance
    );

    glm::vec3 basePosition = vWidth * glm::vec3(-0.35f, 0.0f, -0.35f);

    float triangleTransform[3][4] = {
        { vWidth.x, 0.0f, 0.0f, basePosition.x },
        { 0.0f, vWidth.y, 0.0f, basePosition.y },
        { 0.0f, 0.0f, vWidth.z, basePosition.z }
    };

    VkAccelerationStructureInstanceKHR triangleGeomInstance = {};
    memcpy(&triangleGeomInstance.transform.matrix, &triangleTransform, sizeof(triangleTransform));
    triangleGeomInstance.mask = 1;
    triangleGeomInstance.instanceShaderBindingTableRecordOffset = 0;
    triangleGeomInstance.accelerationStructureReference = triangleAccStruct.gpuAddress;


    std::vector<VkAccelerationStructureInstanceKHR> instances;
    instances.push_back(triangleGeomInstance);
    
    float aabbTransform[3][4] =
    {
        { 1.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f, kAabbWidth / 2.0f },
        { 0.0f, 0.0f, 1.0f, 0.0f }
    };
    
    for (size_t index = 0; index < aabbGeometries.size(); ++index) {
        VkAccelerationStructureInstanceKHR aabbGeomInstance = {};
        memcpy(&aabbGeomInstance.transform.matrix, &aabbTransform, sizeof(aabbTransform));
        aabbGeomInstance.mask = 1;
        aabbGeomInstance.instanceShaderBindingTableRecordOffset = 1 + index;
        aabbGeomInstance.accelerationStructureReference = accStructs[index].gpuAddress;
        instances.push_back(aabbGeomInstance);
    }
    
    uint32_t instanceBufferSize = static_cast<uint32_t>(sizeof(VkAccelerationStructureInstanceKHR) * instances.size());
    VulkanBuffer instanceBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, instanceBufferSize, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_helper.copyToBuffer(instanceBuffer, instances.data(), instanceBufferSize);

    VkAccelerationStructureGeometryKHR topLevelGeometry = {};
    topLevelGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    topLevelGeometry.pNext = nullptr;
    topLevelGeometry.flags = 0;
    topLevelGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    topLevelGeometry.geometry = {};
    topLevelGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    topLevelGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
    topLevelGeometry.geometry.instances.data.deviceAddress = instanceBuffer.address;

    VkAccelerationStructureBuildGeometryInfoKHR topAccelerationStructureGeometryInfo = {};
    topAccelerationStructureGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    topAccelerationStructureGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    topAccelerationStructureGeometryInfo.geometryCount = 1;
    topAccelerationStructureGeometryInfo.pGeometries = &topLevelGeometry;

    uint32_t count = (uint32_t)instances.size();

    VkAccelerationStructureBuildSizesInfoKHR topAccelerationStructureSizes;
    topAccelerationStructureSizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(m_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &topAccelerationStructureGeometryInfo, &count, &topAccelerationStructureSizes);

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.size = topAccelerationStructureSizes.accelerationStructureSize;

    VulkanBuffer topAccelerationBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, topAccelerationStructureSizes.accelerationStructureSize, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkAccelerationStructureCreateInfoKHR topAccInfo = {};
    topAccInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    topAccInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    topAccInfo.buffer = topAccelerationBuffer.handle;
    topAccInfo.offset = 0;
    topAccInfo.size = topAccelerationStructureSizes.accelerationStructureSize;

    VkAccelerationStructureKHR topAccelerationStructure;
    VK_CHECK(vkCreateAccelerationStructureKHR(m_device, &topAccInfo, nullptr, &topAccelerationStructure));

    VkDeviceSize bottomTriangleAccelerationStructureBufferSize = triangleAccelerationStructureSizes.buildScratchSize;

    VkDeviceSize bottomAabbAccelerationStructureBufferSize = 0;

    for (size_t index = 0; index < accStructs.size(); ++index) {
        bottomAabbAccelerationStructureBufferSize = std::max(aabbAsBuildSizes[index].buildScratchSize, bottomAabbAccelerationStructureBufferSize);
    }

    VkDeviceSize scratchBufferSize = std::max(bottomTriangleAccelerationStructureBufferSize, bottomAabbAccelerationStructureBufferSize);
    VulkanBuffer scratchBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, scratchBufferSize, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkCommandBufferAllocateInfo commandBufferAllocInfo = {};
    commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocInfo.commandPool = m_commandPool;
    commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer;
    VK_CHECK(vkAllocateCommandBuffers(m_device, &commandBufferAllocInfo, &cmdBuffer));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    VkAccelerationStructureBuildRangeInfoKHR triangleBuildRangeInfo = {};
    triangleBuildRangeInfo.primitiveCount = 2;
    triangleBuildRangeInfo.primitiveOffset = 0;
    triangleBuildRangeInfo.firstVertex = 0;
    triangleBuildRangeInfo.transformOffset = 0;

    {
        VkAccelerationStructureBuildGeometryInfoKHR asBuildInfo = {};
        asBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        asBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        asBuildInfo.geometryCount = 1;
        asBuildInfo.pGeometries = &triangleGeometry;
        asBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        asBuildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
        asBuildInfo.dstAccelerationStructure = triangleAccStruct.handle;
        asBuildInfo.scratchData.deviceAddress = scratchBuffer.address;

        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> asOffsetInfos = { &triangleBuildRangeInfo };

        //VK_CHECK(vkBuildAccelerationStructureKHR(m_device, 1, &asBuildInfo, asOffsetInfos.data()));
        vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &asBuildInfo, asOffsetInfos.data());
    }
    
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
    
    VkAccelerationStructureBuildRangeInfoKHR aabbBuildRangeInfo = {};
    aabbBuildRangeInfo.primitiveCount = 1;
    aabbBuildRangeInfo.primitiveOffset = 0;
    aabbBuildRangeInfo.firstVertex = 0;
    aabbBuildRangeInfo.transformOffset = 0;

    for (size_t index = 0; index < accStructs.size(); ++index)
    {
        VkAccelerationStructureBuildGeometryInfoKHR asBuildInfo = {};
        asBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        asBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        asBuildInfo.geometryCount = 1;//aabbGeometries.size();
        asBuildInfo.pGeometries = &aabbGeometries[index];
        asBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        asBuildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
        asBuildInfo.dstAccelerationStructure = accStructs[index].handle;
        asBuildInfo.scratchData.deviceAddress = scratchBuffer.address;

        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> asOffsetInfos = { &aabbBuildRangeInfo };

        //VK_CHECK(vkBuildAccelerationStructureKHR(m_device, 1, &asBuildInfo, asOffsetInfos.data()));
        vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &asBuildInfo, asOffsetInfos.data());

        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
    }

    VkAccelerationStructureBuildRangeInfoKHR topLevelBuildRangeInfo = {};
    topLevelBuildRangeInfo.primitiveCount = static_cast<uint32_t>(instances.size());
    topLevelBuildRangeInfo.primitiveOffset = 0;
    topLevelBuildRangeInfo.firstVertex = 0;
    topLevelBuildRangeInfo.transformOffset = 0;

    {
        VkAccelerationStructureBuildGeometryInfoKHR asBuildInfo = {};
        asBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        asBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        asBuildInfo.geometryCount = 1;
        asBuildInfo.pGeometries = &topLevelGeometry;
        asBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        asBuildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
        asBuildInfo.dstAccelerationStructure = topAccelerationStructure;
        asBuildInfo.scratchData.deviceAddress = scratchBuffer.address;

        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> asOffsetInfos = { &topLevelBuildRangeInfo };

        //VK_CHECK(vkBuildAccelerationStructureKHR(m_device, 1, &asBuildInfo, asOffsetInfos.data()));
        vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &asBuildInfo, asOffsetInfos.data());
    }

    VK_CHECK(vkEndCommandBuffer(cmdBuffer));

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &fence));

    VK_CHECK(vkQueueSubmit(m_queue, 1, &submitInfo, fence));
    VK_CHECK(vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX));

    vkDestroyFence(m_device, fence, nullptr);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmdBuffer);

    m_topLevelAs = topAccelerationStructure;
}

void CRayTracing::updateCameraMatrices() {
    m_sceneCB.cameraPosition = m_eye;
    float fovAngleY = 45.0f;
    glm::mat4 view = glm::lookAtLH(glm::vec3(m_eye), glm::vec3(m_at), glm::vec3(m_up));
    glm::mat4 proj = glm::perspectiveLH(glm::radians(fovAngleY), m_aspectRatio, 0.01f, 125.0f);
    glm::mat4 viewProj = proj * view;
    m_sceneCB.projectionToWorld = glm::inverse(viewProj);
}

void CRayTracing::updateAABBPrimitivesAttributes(float animationTime) {
    glm::mat4 identity = glm::mat4(1.0f);

    glm::mat4 scale15y = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.5f, 1.0f));
    glm::mat4 scale15 = glm::scale(glm::mat4(1.0f), glm::vec3(1.5f, 1.5f, 1.5f));
    //glm::mat4 scale2 = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 2.0f, 2.0f));
    glm::mat4 scale3 = glm::scale(glm::mat4(1.0f), glm::vec3(3.0f, 3.0f, 3.0f));

    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), -2.0f * animationTime, glm::vec3(0.0f, 1.0f, 0.0f));

    auto setTransformAABB = [&](uint32_t primitiveIndex, glm::mat4& this_scale, glm::mat4& this_rotation) {
      glm::vec3 vTranslation =
              0.5f * (glm::vec3(m_aabbs[primitiveIndex].minX, m_aabbs[primitiveIndex].minY, m_aabbs[primitiveIndex].minZ)
                      + glm::vec3(m_aabbs[primitiveIndex].maxX, m_aabbs[primitiveIndex].maxY, m_aabbs[primitiveIndex].maxZ));
      glm::mat4 translation = glm::translate(glm::mat4(1.0f), vTranslation);

      glm::mat4 transform = translation * this_rotation * this_scale;
      m_aabbPrimitiveAttributeBuffer[primitiveIndex].localSpaceToBottomLevelAS = transform;
      m_aabbPrimitiveAttributeBuffer[primitiveIndex].bottomLevelASToLocalSpace = glm::inverse(transform);
    };

    uint32_t offset = 0;

    {
        setTransformAABB(offset + AnalyticPrimitive::AABB, scale15y, identity);
        setTransformAABB(offset + AnalyticPrimitive::Spheres, scale15, rotation);
        offset += AnalyticPrimitive::Count;
    }

    {
        setTransformAABB(offset + VolumetricPrimitive::Metaballs, scale15, rotation);
        offset += VolumetricPrimitive::Count;
    }

    {
        setTransformAABB(offset + SignedDistancePrimitive::MiniSpheres, identity, identity);
        setTransformAABB(offset + SignedDistancePrimitive::IntersectedRoundCube, identity, identity);
        setTransformAABB(offset + SignedDistancePrimitive::SquareTorus, scale15, identity);
        setTransformAABB(offset + SignedDistancePrimitive::TwistedTorus, identity, rotation);
        setTransformAABB(offset + SignedDistancePrimitive::Cog, identity, rotation);
        setTransformAABB(offset + SignedDistancePrimitive::Cylinder, scale15y, identity);
        setTransformAABB(offset + SignedDistancePrimitive::FractalPyramid, scale3, identity);
    }
}

void CRayTracing::buildProceduralGeometryAABBs() {

    {
        glm::ivec3 aabbGrid = glm::ivec3(4, 1, 4);
        glm::vec3 const basePosition = glm::vec3(
            -(aabbGrid.x * kAabbWidth + (aabbGrid.x - 1) * kAabbDistance) / 2.0f,
            -(aabbGrid.y * kAabbWidth + (aabbGrid.y - 1) * kAabbDistance) / 2.0f,
            -(aabbGrid.z * kAabbWidth + (aabbGrid.z - 1) * kAabbDistance) / 2.0f
        );

        glm::vec3 stride = glm::vec3(kAabbWidth + kAabbDistance, kAabbWidth + kAabbDistance, kAabbWidth + kAabbDistance);

        auto initializeAABB = [&](glm::vec3 const& offsetIndex, glm::vec3 const& size) {
            return VkAabbPositionsKHR {
                basePosition.x + offsetIndex.x * stride.x,
                basePosition.y + offsetIndex.y * stride.y,
                basePosition.z + offsetIndex.z * stride.z,
                basePosition.x + offsetIndex.x * stride.x + size.x,
                basePosition.y + offsetIndex.y * stride.y + size.y,
                basePosition.z + offsetIndex.z * stride.z + size.z,
            };
        };

        m_aabbs.resize(IntersectionShaderType::kTotalPrimitiveCount);

        uint32_t offset = 0;

        {
            m_aabbs[offset + AnalyticPrimitive::AABB] = initializeAABB(glm::ivec3(3, 0, 0), glm::vec3(2.0f, 3.0f, 2.0f));
            m_aabbs[offset + AnalyticPrimitive::Spheres] = initializeAABB(glm::vec3(2.25f, 0.0f, 0.75f), glm::vec3(3.0f, 3.0f, 3.0f));
            offset += AnalyticPrimitive::Count;
        }

        {
            m_aabbs[offset + VolumetricPrimitive::Metaballs] = initializeAABB(glm::ivec3(0, 0, 0), glm::vec3(3.0f, 3.0f, 3.0f));
            offset += VolumetricPrimitive::Count;
        }

        {
            m_aabbs[offset + SignedDistancePrimitive::MiniSpheres] = initializeAABB(glm::ivec3(2, 0, 0), glm::vec3(2.0f, 2.0f, 2.0f));
            m_aabbs[offset + SignedDistancePrimitive::TwistedTorus] = initializeAABB(glm::ivec3(0, 0, 1), glm::vec3(2.0f, 2.0f, 2.0f));
            m_aabbs[offset + SignedDistancePrimitive::IntersectedRoundCube] = initializeAABB(glm::ivec3(0, 0, 2), glm::vec3(2.0f, 2.0f, 2.0f));
            m_aabbs[offset + SignedDistancePrimitive::SquareTorus] = initializeAABB(glm::vec3(0.75f, -0.1f, 2.25f), glm::vec3(3.0f, 3.0f, 3.0f));
            m_aabbs[offset + SignedDistancePrimitive::Cog] = initializeAABB(glm::ivec3(1, 0, 0), glm::vec3(2.0f, 2.0f, 2.0f));
            m_aabbs[offset + SignedDistancePrimitive::Cylinder] = initializeAABB(glm::ivec3(0, 0, 3), glm::vec3(2.0f, 3.0f, 2.0f));
            m_aabbs[offset + SignedDistancePrimitive::FractalPyramid] = initializeAABB(glm::ivec3(2, 0, 2), glm::vec3(6.0f, 6.0f, 6.0f));
        }

        for (size_t index = 0; index < m_aabbs.size(); ++index) {
            VulkanBuffer aabbBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, sizeof(VkAabbPositionsKHR), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            m_helper.copyToBuffer(aabbBuffer, &m_aabbs[index], sizeof(VkAabbPositionsKHR));
            m_aabbBuffers.push_back(aabbBuffer);
        }
    }
}

void CRayTracing::buildPlaneGeometry() {
    Index indices[] = {
        3, 1, 0,
        2, 1, 3
    };

    Vertex vertices[] = {
        { glm::vec3(0.0f, 0.0f, 0.0f) },
        { glm::vec3(1.0f, 0.0f, 0.0f) },
        { glm::vec3(1.0f, 0.0f, 1.0f) },
        { glm::vec3(0.0f, 0.0f, 1.0f) },
    };

    m_indexBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, sizeof(indices), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_helper.copyToBuffer(m_indexBuffer, indices, sizeof(indices));
    m_vertexBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, sizeof(vertices), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_helper.copyToBuffer(m_vertexBuffer, vertices, sizeof(vertices));

    std::vector<uint32_t> faces;
    for(uint32_t index = 0; index < sizeof(indices) / sizeof(Index); index += 3) {
        faces.push_back(indices[index + 0]);
        faces.push_back(indices[index + 1]);
        faces.push_back(indices[index + 2]);
        faces.push_back(0);
    }

    m_facesBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, faces.size() * sizeof(uint32_t), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_helper.copyToBuffer(m_facesBuffer, faces.data(), static_cast<uint32_t>(faces.size() * sizeof(uint32_t)));

    glm::vec4 normals[] = {
        glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
        glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
        glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
        glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
    };

    m_normalBuffer = m_helper.createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, sizeof(normals), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_helper.copyToBuffer(m_normalBuffer, normals, sizeof(normals));
    // TODO: create buffers
}

void CRayTracing::update() {

    float elapsedTime = 1.0f / 60.0f;

    // Rotate the camera around Y axis.
    {

        if (m_animateCamera)
        {
            float secondsToRotateAround = 48.0f;
            float angleToRotateBy = 360.0f * (elapsedTime / secondsToRotateAround);
            glm::mat4 rotate = glm::rotate(glm::mat4(1.0), glm::radians(angleToRotateBy), glm::vec3(0.0f, 1.0f, 0.0f));
            m_eye = rotate * m_eye;
            m_up = rotate * m_up;
            m_at = rotate * m_at;
            updateCameraMatrices();
        }


        if (m_animateLight)
        {
            float secondsToRotateAround = 8.0f;
            float angleToRotateBy = -360.0f * (elapsedTime / secondsToRotateAround);
            glm::mat4 rotate = glm::rotate(glm::mat4(1.0), glm::radians(angleToRotateBy), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::vec4 prevLightPosition = m_sceneCB.lightPosition;
            m_sceneCB.lightPosition = rotate * prevLightPosition;
        }

        static float animateGeometryTime = 0.0f;
        animateGeometryTime += elapsedTime;

        updateAABBPrimitivesAttributes(animateGeometryTime);

        m_sceneCB.elapsedTime = animateGeometryTime;

        updateSceneBuffer();

        updateAABBPrimitiveBuffer();

    }
}
