#include "vulkanhelper.hxx"

#include <string.h>
#include <stdio.h>

#if defined(__linux__)
#include <dlfcn.h>
#endif

#define DEFINE_VK_FUNCTION(name) \
PFN_##name name = nullptr

#define INIT_VK_GLOBAL_FUNCTION(methodname) \
   do { \
       methodname = reinterpret_cast<PFN_##methodname>(vkGetInstanceProcAddr(nullptr, #methodname)); \
       if (!methodname) { \
           printf("Could not get pointer for %s pointer\n", #methodname); \
       } \
   } while (0)

#define INIT_VK_INSTANCE_FUNCTION(methodname) \
   do { \
       methodname = reinterpret_cast<PFN_##methodname>(vkGetInstanceProcAddr(instance, #methodname)); \
       if (!methodname) { \
           printf("Could not get pointer for %s pointer\n", #methodname); \
       } \
   } while (0)

#define INIT_VK_DEVICE_FUNCTION(methodname) \
   do { \
       methodname = reinterpret_cast<PFN_##methodname>(vkGetDeviceProcAddr(device, #methodname)); \
       if (!methodname) { \
           printf("Could not get pointer for %s pointer\n", #methodname); \
       } \
   } while (0)

/*
 * Vulkan Core functions
 */

DEFINE_VK_FUNCTION(vkGetInstanceProcAddr);
DEFINE_VK_FUNCTION(vkGetPhysicalDeviceMemoryProperties);
DEFINE_VK_FUNCTION(vkGetDeviceProcAddr);
DEFINE_VK_FUNCTION(vkCreateShaderModule);
DEFINE_VK_FUNCTION(vkMapMemory);
DEFINE_VK_FUNCTION(vkUnmapMemory);
DEFINE_VK_FUNCTION(vkCreateImage);
DEFINE_VK_FUNCTION(vkGetImageMemoryRequirements);
DEFINE_VK_FUNCTION(vkAllocateMemory);
DEFINE_VK_FUNCTION(vkBindImageMemory);
DEFINE_VK_FUNCTION(vkCreateImageView);
DEFINE_VK_FUNCTION(vkAllocateCommandBuffers);
DEFINE_VK_FUNCTION(vkBeginCommandBuffer);
DEFINE_VK_FUNCTION(vkUpdateDescriptorSets);
DEFINE_VK_FUNCTION(vkCmdPipelineBarrier);
DEFINE_VK_FUNCTION(vkEndCommandBuffer);
DEFINE_VK_FUNCTION(vkQueueSubmit);
DEFINE_VK_FUNCTION(vkQueueWaitIdle);
DEFINE_VK_FUNCTION(vkFreeCommandBuffers);
DEFINE_VK_FUNCTION(vkCreateBuffer);
DEFINE_VK_FUNCTION(vkGetBufferMemoryRequirements);
DEFINE_VK_FUNCTION(vkBindBufferMemory);
DEFINE_VK_FUNCTION(vkEnumerateInstanceLayerProperties);
DEFINE_VK_FUNCTION(vkEnumerateInstanceExtensionProperties);
DEFINE_VK_FUNCTION(vkCreateInstance);
DEFINE_VK_FUNCTION(vkEnumeratePhysicalDevices);
DEFINE_VK_FUNCTION(vkEnumerateDeviceLayerProperties);
DEFINE_VK_FUNCTION(vkEnumerateDeviceExtensionProperties);
DEFINE_VK_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties);
DEFINE_VK_FUNCTION(vkGetPhysicalDeviceFeatures);
DEFINE_VK_FUNCTION(vkGetPhysicalDeviceFeatures2);
DEFINE_VK_FUNCTION(vkCreateDevice);
DEFINE_VK_FUNCTION(vkGetDeviceQueue);
DEFINE_VK_FUNCTION(vkCreateCommandPool);
DEFINE_VK_FUNCTION(vkCreateDescriptorPool);
DEFINE_VK_FUNCTION(vkGetPhysicalDeviceProperties);
DEFINE_VK_FUNCTION(vkGetPhysicalDeviceProperties2);
DEFINE_VK_FUNCTION(vkCreateDescriptorSetLayout);
DEFINE_VK_FUNCTION(vkAllocateDescriptorSets);
DEFINE_VK_FUNCTION(vkCreatePipelineLayout);
DEFINE_VK_FUNCTION(vkCreateRenderPass);
DEFINE_VK_FUNCTION(vkCreateFramebuffer);
DEFINE_VK_FUNCTION(vkCmdBindPipeline);
DEFINE_VK_FUNCTION(vkCmdBindDescriptorSets);
DEFINE_VK_FUNCTION(vkCmdCopyImage);
DEFINE_VK_FUNCTION(vkCreateSemaphore);
DEFINE_VK_FUNCTION(vkCreateFence);
DEFINE_VK_FUNCTION(vkWaitForFences);
DEFINE_VK_FUNCTION(vkResetFences);

/*
 * Vulkan WSI functions
 */
#ifdef VK_USE_PLATFORM_WIN32_KHR
DEFINE_VK_FUNCTION(vkCreateWin32SurfaceKHR);
#endif

#ifdef VK_USE_PLATFORM_XCB_KHR
DEFINE_VK_FUNCTION(vkCreateXcbSurfaceKHR);
#endif

/*
 * Vulkan Surface extension functions
 */

DEFINE_VK_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR);
DEFINE_VK_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR);
DEFINE_VK_FUNCTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
DEFINE_VK_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR);
DEFINE_VK_FUNCTION(vkCreateSwapchainKHR);
DEFINE_VK_FUNCTION(vkGetSwapchainImagesKHR);
DEFINE_VK_FUNCTION(vkAcquireNextImageKHR);
DEFINE_VK_FUNCTION(vkQueuePresentKHR);

/*
 * Vulkan NVIDIA Raytracing extension functions
 */
DEFINE_VK_FUNCTION(vkCreateAccelerationStructureNV);
DEFINE_VK_FUNCTION(vkDestroyAccelerationStructureNV);
DEFINE_VK_FUNCTION(vkGetAccelerationStructureMemoryRequirementsNV);
DEFINE_VK_FUNCTION(vkBindAccelerationStructureMemoryNV);
DEFINE_VK_FUNCTION(vkCmdBuildAccelerationStructureNV);
DEFINE_VK_FUNCTION(vkCmdCopyAccelerationStructureNV);
DEFINE_VK_FUNCTION(vkCmdTraceRaysNV);
DEFINE_VK_FUNCTION(vkCreateRayTracingPipelinesNV);
DEFINE_VK_FUNCTION(vkGetRayTracingShaderGroupHandlesNV);
DEFINE_VK_FUNCTION(vkGetAccelerationStructureHandleNV);
DEFINE_VK_FUNCTION(vkCmdWriteAccelerationStructuresPropertiesNV);
DEFINE_VK_FUNCTION(vkCompileDeferredNV);

static void initVulkanDynamicLoadLibrary() {
#if defined(WIN32)
	HMODULE vulkanLibrary = LoadLibrary("vulkan-1.dll");
	vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress(vulkanLibrary, "vkGetInstanceProcAddr");
#elif defined(__linux__)
	void* vulkanLibrary = dlopen("libvulkan.so", RTLD_NOW);
	vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(vulkanLibrary, "vkGetInstanceProcAddr");
#endif
}

CVulkanHelper::CVulkanHelper(VkInstance instance, VkDevice device, VkPhysicalDevice gpu)
    : m_instance(instance)
	, m_device(device)
    , m_gpu(gpu)
{
    vkGetPhysicalDeviceMemoryProperties(gpu, &m_gpuMemoryProperties);
}

void CVulkanHelper::initVulkan() {
	initVulkanDynamicLoadLibrary();

	INIT_VK_GLOBAL_FUNCTION(vkEnumerateInstanceLayerProperties);
	INIT_VK_GLOBAL_FUNCTION(vkEnumerateInstanceExtensionProperties);
	INIT_VK_GLOBAL_FUNCTION(vkCreateInstance);
}

void CVulkanHelper::initVulkanInstanceFunctions(VkInstance instance) {
	INIT_VK_INSTANCE_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties);
	INIT_VK_INSTANCE_FUNCTION(vkGetPhysicalDeviceFeatures);
	INIT_VK_INSTANCE_FUNCTION(vkGetPhysicalDeviceFeatures2);
	INIT_VK_INSTANCE_FUNCTION(vkGetPhysicalDeviceProperties);
	INIT_VK_INSTANCE_FUNCTION(vkGetPhysicalDeviceProperties2);
	INIT_VK_INSTANCE_FUNCTION(vkEnumeratePhysicalDevices);
	INIT_VK_INSTANCE_FUNCTION(vkCreateDevice);
	INIT_VK_INSTANCE_FUNCTION(vkGetDeviceProcAddr);
	INIT_VK_INSTANCE_FUNCTION(vkEnumerateDeviceLayerProperties);
	INIT_VK_INSTANCE_FUNCTION(vkEnumerateDeviceExtensionProperties);
	INIT_VK_INSTANCE_FUNCTION(vkGetPhysicalDeviceMemoryProperties);


#ifdef VK_USE_PLATFORM_WIN32_KHR
	INIT_VK_INSTANCE_FUNCTION(vkCreateWin32SurfaceKHR);
#endif

#ifdef VK_USE_PLATFORM_XCB_KHR
	INIT_VK_INSTANCE_FUNCTION(vkCreateXcbSurfaceKHR);
#endif

	INIT_VK_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR);
	INIT_VK_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR);
	INIT_VK_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
	INIT_VK_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR);
}

void CVulkanHelper::initVulkanDeviceFunctions(VkDevice device) {
	INIT_VK_DEVICE_FUNCTION(vkCreateShaderModule);
	INIT_VK_DEVICE_FUNCTION(vkMapMemory);
	INIT_VK_DEVICE_FUNCTION(vkUnmapMemory);
	INIT_VK_DEVICE_FUNCTION(vkCreateImage);
	INIT_VK_DEVICE_FUNCTION(vkGetImageMemoryRequirements);
	INIT_VK_DEVICE_FUNCTION(vkAllocateMemory);
	INIT_VK_DEVICE_FUNCTION(vkBindImageMemory);
	INIT_VK_DEVICE_FUNCTION(vkCreateImageView);
	INIT_VK_DEVICE_FUNCTION(vkAllocateCommandBuffers);
	INIT_VK_DEVICE_FUNCTION(vkBeginCommandBuffer);
	INIT_VK_DEVICE_FUNCTION(vkUpdateDescriptorSets);
	INIT_VK_DEVICE_FUNCTION(vkCmdPipelineBarrier);
	INIT_VK_DEVICE_FUNCTION(vkEndCommandBuffer);
	INIT_VK_DEVICE_FUNCTION(vkQueueSubmit);
	INIT_VK_DEVICE_FUNCTION(vkQueueWaitIdle);
	INIT_VK_DEVICE_FUNCTION(vkFreeCommandBuffers);
	INIT_VK_DEVICE_FUNCTION(vkCreateBuffer);
	INIT_VK_DEVICE_FUNCTION(vkGetBufferMemoryRequirements);
	INIT_VK_DEVICE_FUNCTION(vkBindBufferMemory);
	INIT_VK_DEVICE_FUNCTION(vkGetDeviceQueue);
	INIT_VK_DEVICE_FUNCTION(vkCreateCommandPool);
	INIT_VK_DEVICE_FUNCTION(vkCreateDescriptorPool);
	INIT_VK_DEVICE_FUNCTION(vkCreateDescriptorSetLayout);
	INIT_VK_DEVICE_FUNCTION(vkAllocateDescriptorSets);
	INIT_VK_DEVICE_FUNCTION(vkCreatePipelineLayout);
	INIT_VK_DEVICE_FUNCTION(vkCreateRenderPass);
	INIT_VK_DEVICE_FUNCTION(vkCreateFramebuffer);
	INIT_VK_DEVICE_FUNCTION(vkCmdBindPipeline);
	INIT_VK_DEVICE_FUNCTION(vkCmdBindDescriptorSets);
	INIT_VK_DEVICE_FUNCTION(vkCmdCopyImage);
	INIT_VK_DEVICE_FUNCTION(vkCreateSemaphore);
	INIT_VK_DEVICE_FUNCTION(vkCreateFence);
	INIT_VK_DEVICE_FUNCTION(vkWaitForFences);
	INIT_VK_DEVICE_FUNCTION(vkResetFences);


	INIT_VK_DEVICE_FUNCTION(vkCreateSwapchainKHR);
	INIT_VK_DEVICE_FUNCTION(vkGetSwapchainImagesKHR);
	INIT_VK_DEVICE_FUNCTION(vkAcquireNextImageKHR);
	INIT_VK_DEVICE_FUNCTION(vkQueuePresentKHR);


	INIT_VK_DEVICE_FUNCTION(vkCreateAccelerationStructureNV);
	INIT_VK_DEVICE_FUNCTION(vkDestroyAccelerationStructureNV);
	INIT_VK_DEVICE_FUNCTION(vkGetAccelerationStructureMemoryRequirementsNV);
	INIT_VK_DEVICE_FUNCTION(vkBindAccelerationStructureMemoryNV);
	INIT_VK_DEVICE_FUNCTION(vkCmdBuildAccelerationStructureNV);
	INIT_VK_DEVICE_FUNCTION(vkCmdCopyAccelerationStructureNV);
	INIT_VK_DEVICE_FUNCTION(vkCmdTraceRaysNV);
	INIT_VK_DEVICE_FUNCTION(vkCreateRayTracingPipelinesNV);
	INIT_VK_DEVICE_FUNCTION(vkGetRayTracingShaderGroupHandlesNV);
	INIT_VK_DEVICE_FUNCTION(vkGetAccelerationStructureHandleNV);
	INIT_VK_DEVICE_FUNCTION(vkCmdWriteAccelerationStructuresPropertiesNV);
	INIT_VK_DEVICE_FUNCTION(vkCompileDeferredNV);
}

uint32_t CVulkanHelper::getMemoryType(VkMemoryRequirements& memoryRequirements,
                                      VkMemoryPropertyFlags memoryProperties) {
    uint32_t memoryType = 0;
    for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < VK_MAX_MEMORY_TYPES; ++memoryTypeIndex) {
        if (memoryRequirements.memoryTypeBits & (1 << memoryTypeIndex)) {
            if ((m_gpuMemoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & memoryProperties) == memoryProperties) {
                memoryType = memoryTypeIndex;
                break;
            }
        }
    }

    return memoryType;
}

VulkanBuffer CVulkanHelper::createBuffer(VkBufferUsageFlags usage,
                                     VkDeviceSize size,
                                     VkMemoryPropertyFlags memoryProperties) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.usage = usage;
    bufferInfo.size = size;

    VkBuffer buffer;
    vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer);

    VkMemoryRequirements bufferMemoryRequirements;
    vkGetBufferMemoryRequirements(m_device, buffer, &bufferMemoryRequirements);

    uint32_t memoryType = getMemoryType(bufferMemoryRequirements, memoryProperties);

    VkMemoryAllocateInfo memoryAllocInfo = {};
    memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocInfo.allocationSize = bufferMemoryRequirements.size;
    memoryAllocInfo.memoryTypeIndex = memoryType;

    VkDeviceMemory bufferMemory;
    vkAllocateMemory(m_device, &memoryAllocInfo, nullptr, &bufferMemory);

    vkBindBufferMemory(m_device, buffer, bufferMemory, 0);
    return {buffer, bufferMemory, size};
}

void CVulkanHelper::copyToBuffer(const VulkanBuffer &buffer, void* data, uint32_t size) {

    void* localData = nullptr;
    vkMapMemory(m_device, buffer.memory, 0, buffer.size, 0, &localData);
    memcpy(localData, data, size);
    vkUnmapMemory(m_device, buffer.memory);
}
