#ifndef VULKANHELPER_HXX
#define VULKANHELPER_HXX

#define VK_NO_PROTOTYPES
#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>

#define VK_CHECK(func) \
do { \
    VkResult res = (func); \
    if (res != VK_SUCCESS) { \
        printf("function %s failed with %d\n", #func, res); \
    } \
} while(0)

#define EXTERN_VK_FUNCTION(name) \
extern PFN_##name name

/*
 * Vulkan 1.2 Core functions
 */

EXTERN_VK_FUNCTION(vkGetInstanceProcAddr);
EXTERN_VK_FUNCTION(vkGetPhysicalDeviceMemoryProperties);
EXTERN_VK_FUNCTION(vkGetDeviceProcAddr);
EXTERN_VK_FUNCTION(vkCreateShaderModule);
EXTERN_VK_FUNCTION(vkMapMemory);
EXTERN_VK_FUNCTION(vkUnmapMemory);
EXTERN_VK_FUNCTION(vkCreateImage);
EXTERN_VK_FUNCTION(vkGetImageMemoryRequirements);
EXTERN_VK_FUNCTION(vkAllocateMemory);
EXTERN_VK_FUNCTION(vkBindImageMemory);
EXTERN_VK_FUNCTION(vkCreateImageView);
EXTERN_VK_FUNCTION(vkAllocateCommandBuffers);
EXTERN_VK_FUNCTION(vkBeginCommandBuffer);
EXTERN_VK_FUNCTION(vkUpdateDescriptorSets);
EXTERN_VK_FUNCTION(vkCmdPipelineBarrier);
EXTERN_VK_FUNCTION(vkEndCommandBuffer);
EXTERN_VK_FUNCTION(vkQueueSubmit);
EXTERN_VK_FUNCTION(vkQueueWaitIdle);
EXTERN_VK_FUNCTION(vkFreeCommandBuffers);
EXTERN_VK_FUNCTION(vkCreateBuffer);
EXTERN_VK_FUNCTION(vkGetBufferMemoryRequirements);
EXTERN_VK_FUNCTION(vkBindBufferMemory);
EXTERN_VK_FUNCTION(vkEnumerateInstanceLayerProperties);
EXTERN_VK_FUNCTION(vkEnumerateInstanceExtensionProperties);
EXTERN_VK_FUNCTION(vkCreateInstance);
EXTERN_VK_FUNCTION(vkEnumeratePhysicalDevices);
EXTERN_VK_FUNCTION(vkEnumerateDeviceLayerProperties);
EXTERN_VK_FUNCTION(vkEnumerateDeviceExtensionProperties);
EXTERN_VK_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties);
EXTERN_VK_FUNCTION(vkGetPhysicalDeviceFeatures);
EXTERN_VK_FUNCTION(vkGetPhysicalDeviceFeatures2);
EXTERN_VK_FUNCTION(vkCreateDevice);
EXTERN_VK_FUNCTION(vkGetDeviceQueue);
EXTERN_VK_FUNCTION(vkCreateCommandPool);
EXTERN_VK_FUNCTION(vkCreateDescriptorPool);
EXTERN_VK_FUNCTION(vkGetPhysicalDeviceProperties);
EXTERN_VK_FUNCTION(vkGetPhysicalDeviceProperties2);
EXTERN_VK_FUNCTION(vkCreateDescriptorSetLayout);
EXTERN_VK_FUNCTION(vkAllocateDescriptorSets);
EXTERN_VK_FUNCTION(vkCreatePipelineLayout);
EXTERN_VK_FUNCTION(vkCreateRenderPass);
EXTERN_VK_FUNCTION(vkCreateFramebuffer);
EXTERN_VK_FUNCTION(vkCmdBindPipeline);
EXTERN_VK_FUNCTION(vkCmdBindDescriptorSets);
EXTERN_VK_FUNCTION(vkCmdCopyImage);
EXTERN_VK_FUNCTION(vkCreateSemaphore);
EXTERN_VK_FUNCTION(vkCreateFence);
EXTERN_VK_FUNCTION(vkWaitForFences);
EXTERN_VK_FUNCTION(vkResetFences);
EXTERN_VK_FUNCTION(vkGetBufferDeviceAddress);
EXTERN_VK_FUNCTION(vkDestroyFence);

/*
 * Vulkan WSI functions
 */
#ifdef VK_USE_PLATFORM_WIN32_KHR
EXTERN_VK_FUNCTION(vkCreateWin32SurfaceKHR);
#endif

#ifdef VK_USE_PLATFORM_XCB_KHR
EXTERN_VK_FUNCTION(vkCreateXcbSurfaceKHR);
#endif

/*
 * Vulkan Surface extension functions
 */

EXTERN_VK_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR);
EXTERN_VK_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR);
EXTERN_VK_FUNCTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
EXTERN_VK_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR);
EXTERN_VK_FUNCTION(vkCreateSwapchainKHR);
EXTERN_VK_FUNCTION(vkGetSwapchainImagesKHR);
EXTERN_VK_FUNCTION(vkAcquireNextImageKHR);
EXTERN_VK_FUNCTION(vkQueuePresentKHR);

/*
 * Vulkan NVIDIA Raytracing extension functions
 */
EXTERN_VK_FUNCTION(vkCreateAccelerationStructureNV);
EXTERN_VK_FUNCTION(vkDestroyAccelerationStructureNV);
EXTERN_VK_FUNCTION(vkGetAccelerationStructureMemoryRequirementsNV);
EXTERN_VK_FUNCTION(vkBindAccelerationStructureMemoryNV);
EXTERN_VK_FUNCTION(vkCmdBuildAccelerationStructureNV);
EXTERN_VK_FUNCTION(vkCmdCopyAccelerationStructureNV);
EXTERN_VK_FUNCTION(vkCmdTraceRaysNV);
EXTERN_VK_FUNCTION(vkCreateRayTracingPipelinesNV);
EXTERN_VK_FUNCTION(vkGetRayTracingShaderGroupHandlesNV);
EXTERN_VK_FUNCTION(vkGetAccelerationStructureHandleNV);
EXTERN_VK_FUNCTION(vkCmdWriteAccelerationStructuresPropertiesNV);
EXTERN_VK_FUNCTION(vkCompileDeferredNV);

/*
 * Vulkan KHR Acceleration Structure extension functions
 */
EXTERN_VK_FUNCTION(vkBuildAccelerationStructuresKHR);
EXTERN_VK_FUNCTION(vkCmdBuildAccelerationStructuresIndirectKHR);
EXTERN_VK_FUNCTION(vkCmdBuildAccelerationStructuresKHR);
EXTERN_VK_FUNCTION(vkCmdCopyAccelerationStructureKHR);
EXTERN_VK_FUNCTION(vkCmdCopyAccelerationStructureToMemoryKHR);
EXTERN_VK_FUNCTION(vkCmdCopyMemoryToAccelerationStructureKHR);
EXTERN_VK_FUNCTION(vkCmdWriteAccelerationStructuresPropertiesKHR);
EXTERN_VK_FUNCTION(vkCopyAccelerationStructureKHR);
EXTERN_VK_FUNCTION(vkCopyAccelerationStructureToMemoryKHR);
EXTERN_VK_FUNCTION(vkCopyMemoryToAccelerationStructureKHR);
EXTERN_VK_FUNCTION(vkCreateAccelerationStructureKHR);
EXTERN_VK_FUNCTION(vkDestroyAccelerationStructureKHR);
EXTERN_VK_FUNCTION(vkGetAccelerationStructureBuildSizesKHR);
EXTERN_VK_FUNCTION(vkGetAccelerationStructureDeviceAddressKHR);
EXTERN_VK_FUNCTION(vkGetDeviceAccelerationStructureCompatibilityKHR);
EXTERN_VK_FUNCTION(vkWriteAccelerationStructuresPropertiesKHR);

/*
 * Vulkan KHR Ray Tracing Pipeline extension functions
 */
EXTERN_VK_FUNCTION(vkCmdSetRayTracingPipelineStackSizeKHR);
EXTERN_VK_FUNCTION(vkCmdTraceRaysIndirectKHR);
EXTERN_VK_FUNCTION(vkCmdTraceRaysKHR);
EXTERN_VK_FUNCTION(vkCreateRayTracingPipelinesKHR);
EXTERN_VK_FUNCTION(vkGetRayTracingCaptureReplayShaderGroupHandlesKHR);
EXTERN_VK_FUNCTION(vkGetRayTracingShaderGroupHandlesKHR);
EXTERN_VK_FUNCTION(vkGetRayTracingShaderGroupStackSizeKHR);

struct VulkanBuffer {
    VkBuffer handle;
    VkDeviceMemory memory;
    VkDeviceSize size;
    VkDeviceAddress address;
};

struct VulkanImage {
    VkImage handle;
    VkDeviceMemory memory;
    VkDeviceSize size;
    VkFormat format;
    uint32_t width;
    uint32_t height;
};

class CVulkanHelper
{
public:
    CVulkanHelper(VkInstance instance, VkDevice device, VkPhysicalDevice gpu);
    static void initVulkan();
    static void initVulkanInstanceFunctions(VkInstance instance);
    static void initVulkanDeviceFunctions(VkDevice device);
    static uint32_t alignTo(uint32_t value, uint32_t alignment);
    VulkanBuffer createBuffer(VkBufferUsageFlags usage,
                          VkDeviceSize size,
                          VkMemoryPropertyFlags memoryProperties);
    uint32_t getMemoryType(VkMemoryRequirements& memoryRequirements,
                           VkMemoryPropertyFlags memoryProperties);

    void copyToBuffer(VulkanBuffer const& buffer, void* data, uint32_t size);
private:
    VkInstance m_instance;
    VkDevice m_device;
    VkPhysicalDevice m_gpu;
    VkPhysicalDeviceMemoryProperties m_gpuMemoryProperties;
};

inline uint32_t CVulkanHelper::alignTo(uint32_t value, uint32_t alignment)
{
    return ((value + (alignment - 1)) & ~(alignment - 1));
}

#endif // VULKANHELPER_HXX
