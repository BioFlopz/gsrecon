#pragma once

#include <vulkan/vulkan.h>

struct VulkanFrame
{
    ~VulkanFrame();

    bool init(VkDevice device, uint32_t graphicsQueueFamily);

    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;

    VkFence renderFence = VK_NULL_HANDLE;

private:
    VkDevice device_ = VK_NULL_HANDLE;
};