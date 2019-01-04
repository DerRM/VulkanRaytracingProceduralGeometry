#ifndef VULKANHELPER_HXX
#define VULKANHELPER_HXX

#include <vulkan/vulkan.h>

struct VulkanBuffer {
    VkBuffer handle;
    VkDeviceMemory memory;
    VkDeviceSize size;
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
    CVulkanHelper(VkDevice device, VkPhysicalDevice gpu);
    void init();
    VulkanBuffer createBuffer(VkBufferUsageFlags usage,
                          VkDeviceSize size,
                          VkMemoryPropertyFlags memoryProperties);
    uint32_t getMemoryType(VkMemoryRequirements& memoryRequirements,
                           VkMemoryPropertyFlags memoryProperties);

    void copyToBuffer(VulkanBuffer const& buffer, void* data, uint32_t size);
private:
    VkDevice m_device;
    VkPhysicalDevice m_gpu;
    VkPhysicalDeviceMemoryProperties m_gpuMemoryProperties;
};

#endif // VULKANHELPER_HXX
