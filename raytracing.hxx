#ifndef RAYTRACING_H
#define RAYTRACING_H

#include <stdio.h>
#include <vector>
#include <string>

//#include "shader.hxx"
#include "vulkanhelper.hxx"
#include "raytracingscenedefines.hxx"

struct BottomLevelAccelerationStructure {
    VulkanBuffer buffer;
    VkAccelerationStructureKHR handle;
    VkDeviceAddress gpuAddress;
};

class CRayTracing
{
public:
    CRayTracing(VkInstance instance, VkDevice device, VkPhysicalDevice gpu, VkQueue queue, VkCommandPool commandPool, VkPhysicalDeviceRayTracingPipelinePropertiesKHR const& raytracingPipelineProperties);
    void init();
    void initScene();
    VkPipeline createPipeline(VkPipelineLayout pipelineLayout);
    void createCommandBuffers();
    void createShader(VkShaderStageFlagBits type, std::string const& shader_source);
    void createShaderStages();
    //std::vector<VkRayTracingShaderGroupCreateInfoNV> const& createShaderGroups();
    void createSceneBuffer();
    void updateSceneBuffer();
    void createAABBPrimitiveBuffer();
    void updateAABBPrimitiveBuffer();
    void createPrimitives();
    void updateCameraMatrices();
    void updateAABBPrimitivesAttributes(float animationTime);
    void buildProceduralGeometryAABBs();
    void buildPlaneGeometry();
    void buildTriangleAccelerationStructure();
    void buildAccelerationStructurePlane();
    BottomLevelAccelerationStructure createBottomLevelAccelerationStructure(VkAccelerationStructureBuildSizesInfoKHR const& asBuildSizes);

    void updateDescriptors(VkDescriptorSet descriptorSet);
    VulkanImage createOffscreenImage(VkFormat format, uint32_t width, uint32_t height);

    PrimitiveConstantBuffer& getPlaneMaterialBuffer() { return m_planeMaterialCB; }

    VulkanBuffer getRayGenShaderGroups();
    VulkanBuffer getMissShaderGroups();
    VulkanBuffer getHitShaderGroups();

    void update();

private:
    void createRayGenShaderGroups();
    void createMissShaderGroups();
    void createHitShaderGroups();

    void createRayGenShaderTable();
    void createMissShaderTable();
    void createHitShaderTable();

private:
	VkInstance m_instance;
    VkDevice m_device;
    VkPhysicalDevice m_gpu;
    VkQueue m_queue;
    VkCommandPool m_commandPool;
    CVulkanHelper m_helper;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR const& m_raytracingPipelineProperties;
    //std::vector<CShader> m_shaders;
    std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_shaderGroups;

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_rayGenShaderGroups;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_missShaderGroups;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_hitShaderGroups;


    //VkShaderStageFlagBits shaderType2ShaderStage(CShader::EShaderType type);

    VkPhysicalDeviceMemoryProperties m_gpuMemProps;

    uint32_t const kNumBlas = 2;
    float const kAabbWidth = 2.0f;
    float const kAabbDistance = 2.0f;

    float m_aspectRatio = 1280.0f / 720.0f;
    std::vector<VkAabbPositionsKHR> m_aabbs;

    SceneConstantBuffer m_sceneCB;

    PrimitiveInstancePerFrameBuffer m_aabbPrimitiveAttributeBuffer[IntersectionShaderType::kTotalPrimitiveCount];

    PrimitiveConstantBuffer m_planeMaterialCB;
    PrimitiveConstantBuffer m_aabbMaterialCB[IntersectionShaderType::kTotalPrimitiveCount];
    PrimitiveInstanceConstantBuffer m_aabbInstanceCB[IntersectionShaderType::kTotalPrimitiveCount];


    VulkanBuffer m_indexBuffer;
    VulkanBuffer m_vertexBuffer;
    std::vector<VulkanBuffer> m_aabbBuffers;
    VulkanBuffer m_facesBuffer;
    VulkanBuffer m_normalBuffer;

    VulkanBuffer m_raygenShaderGroupBuffer;
    VulkanBuffer m_missShaderGroupBuffer;
    VulkanBuffer m_hitShaderGroupBuffer;

    VulkanBuffer m_sceneBuffer;
    VulkanBuffer m_aabbPrimitiveBuffer;

    VulkanImage m_offscreenImage;

    VkAccelerationStructureKHR m_bottomLevelAS[BottomLevelASType::Count];
    VkAccelerationStructureKHR m_topLevelAs;

    VkPipeline m_raytracingPipeline;

    glm::vec4 m_eye;
    glm::vec4 m_at;
    glm::vec4 m_up;

    bool m_animateCamera = true;
    bool m_animateLight = false;
};

#endif // RAYTRACING_H
