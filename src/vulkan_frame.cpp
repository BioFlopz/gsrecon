
#include "vulkan_frame.hpp"


bool VulkanFrame::init(VkDevice device, uint32_t graphicsQueueFamily)
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsQueueFamily;

    VkCommandPool commandPool = VK_NULL_HANDLE;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
    {
        return false;
    }

    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    if (vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer) != VK_SUCCESS)
    {
        vkDestroyCommandPool(device, commandPool, nullptr);

        return false;
    }

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkFence renderFence = VK_NULL_HANDLE;

    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailable) != VK_SUCCESS)
    {
        vkDestroyCommandPool(device, commandPool, nullptr);
        return false;
    }



    if (vkCreateFence(device, &fenceInfo, nullptr, &renderFence) != VK_SUCCESS)
    {
        vkDestroySemaphore(device, imageAvailable, nullptr);

        vkDestroyCommandPool(device, commandPool, nullptr);

        return false;
    }

    device_ = device;
    this->commandPool = commandPool;
    this->commandBuffer = commandBuffer;

    imageAvailableSemaphore = imageAvailable;
    this->renderFence = renderFence;

    return true;
}


VulkanFrame::~VulkanFrame()
{
    if (renderFence != VK_NULL_HANDLE)
    {
        vkDestroyFence(device_, renderFence, nullptr);
    }


    if (imageAvailableSemaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(device_, imageAvailableSemaphore, nullptr);
    }

    if (commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(device_, commandPool, nullptr);
    }
}