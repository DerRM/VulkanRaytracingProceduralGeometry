#include "vulkanhelper.hxx"

#include <string.h>

CVulkanHelper::CVulkanHelper(VkDevice device, VkPhysicalDevice gpu)
    : m_device(device)
    , m_gpu(gpu)
{
    vkGetPhysicalDeviceMemoryProperties(gpu, &m_gpuMemoryProperties);
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
