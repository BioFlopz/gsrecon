#pragma once

#include <Windows.h>
#include <vulkan/vulkan.h>

#include <array>

class VulkanContext
{
public:
    ~VulkanContext();

    bool init(HINSTANCE hInstance, HWND window);

    VkInstance instance() const
    {
        return instance_;
    }

    VkSurfaceKHR surface() const
    {
        return surface_;
    }

    VkPhysicalDevice physicalDevice() const
    {
        return physicalDevice_;
    }

    VkDevice device() const
    {
        return device_;
    }

    uint32_t graphicsQueueFamily() const
    {
        return graphicsQueueFamily_;
    }

    uint32_t presentQueueFamily() const
    {
        return presentQueueFamily_;
    }
    
    const std::array<uint8_t, VK_UUID_SIZE>& deviceUUID() const
    {
        return deviceUUID_;
    }



private:
    bool selectPhysicalDevice();

    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;

    uint32_t graphicsQueueFamily_ = UINT32_MAX;
    uint32_t presentQueueFamily_ = UINT32_MAX;

    bool createLogicalDevice();

    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;

    void cleanup();

    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;

    std::array<uint8_t, VK_UUID_SIZE> deviceUUID_{};

};