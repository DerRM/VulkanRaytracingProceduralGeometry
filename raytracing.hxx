#ifndef RAYTRACING_H
#define RAYTRACING_H

#include <vulkan/vulkan.h>
#include <stdio.h>
#include <vector>

//#include "shader.hxx"
#include "vulkanhelper.hxx"
#include "raytracingscenedefines.hxx"

#define DECLARE_RAYTRACING(methodname) \
    extern PFN_##methodname##NV methodname

DECLARE_RAYTRACING(vkCreateAccelerationStructure);
DECLARE_RAYTRACING(vkDestroyAccelerationStructure);
DECLARE_RAYTRACING(vkGetAccelerationStructureMemoryRequirements);
DECLARE_RAYTRACING(vkBindAccelerationStructureMemory);
DECLARE_RAYTRACING(vkCmdBuildAccelerationStructure);
DECLARE_RAYTRACING(vkCmdCopyAccelerationStructure);
DECLARE_RAYTRACING(vkCmdTraceRays);
DECLARE_RAYTRACING(vkCreateRayTracingPipelines);
DECLARE_RAYTRACING(vkGetRayTracingShaderGroupHandles);
DECLARE_RAYTRACING(vkGetAccelerationStructureHandle);
DECLARE_RAYTRACING(vkCmdWriteAccelerationStructuresProperties);
DECLARE_RAYTRACING(vkCompileDeferred);

struct BottomLevelAccelerationStructure {
    VkAccelerationStructureNV handle;
    VkDeviceMemory memory;
    uint64_t gpuHandle;
};

class CRayTracing
{
public:
    CRayTracing(VkDevice device, VkPhysicalDevice gpu, VkQueue queue, VkCommandPool commandPool, VkPhysicalDeviceRayTracingPropertiesNV const& raytracingProperties);
    void init();
    void initScene();
    VkPipeline createPipeline(VkPipelineLayout pipelineLayout);
    void createCommandBuffers();
    void createShader(VkShaderStageFlagBits type, std::string const& name, std::string const& shader_source);
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
    BottomLevelAccelerationStructure createBottomLevelAccelerationStructure(VkGeometryNV* geometry, uint32_t geometryCount, VkBuildAccelerationStructureFlagsNV buildFlags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV);

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
    VkDevice m_device;
    VkPhysicalDevice m_gpu;
    VkQueue m_queue;
    VkCommandPool m_commandPool;
    CVulkanHelper m_helper;
    VkPhysicalDeviceRayTracingPropertiesNV const& m_raytracingProperties;
    //std::vector<CShader> m_shaders;
    std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;
    std::vector<VkRayTracingShaderGroupCreateInfoNV> m_shaderGroups;

    std::vector<VkRayTracingShaderGroupCreateInfoNV> m_rayGenShaderGroups;
    std::vector<VkRayTracingShaderGroupCreateInfoNV> m_missShaderGroups;
    std::vector<VkRayTracingShaderGroupCreateInfoNV> m_hitShaderGroups;


    //VkShaderStageFlagBits shaderType2ShaderStage(CShader::EShaderType type);

    VkPhysicalDeviceMemoryProperties m_gpuMemProps;

    uint32_t const kNumBlas = 2;
    float const kAabbWidth = 2.0f;
    float const kAabbDistance = 2.0f;

    float m_aspectRatio = 1280.0f / 720.0f;
    std::vector<AABB> m_aabbs;

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

    VkAccelerationStructureNV m_bottomLevelAS[BottomLevelASType::Count];
    VkAccelerationStructureNV m_topLevelAs;

    VkPipeline m_raytracingPipeline;

    glm::vec4 m_eye;
    glm::vec4 m_at;
    glm::vec4 m_up;

    bool m_animateCamera = false;
    bool m_animateLight = false;
};

#endif // RAYTRACING_H
