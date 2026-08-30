#pragma once

#include <vulkan/vulkan.h>

#include <vector>

class VulkanSwapchain
{
public:
    ~VulkanSwapchain();

    bool init(
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        VkSurfaceKHR surface,
        uint32_t graphicsQueueFamily,
        uint32_t presentQueueFamily,
        uint32_t width,
        uint32_t height
    );

    VkSwapchainKHR handle() const
    {
        return swapchain_;
    }

    const std::vector<VkImage>& images() const
    {
        return images_;
    }

    const std::vector<VkImageView>& imageViews() const
    {
        return imageViews_;
    }

    VkFormat imageFormat() const
    {
        return imageFormat_;
    }

    VkExtent2D extent() const
    {
        return extent_;
    }



private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;

    std::vector<VkImage> images_;
    std::vector<VkImageView> imageViews_;

    VkFormat imageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D extent_{};


    void cleanup();
};