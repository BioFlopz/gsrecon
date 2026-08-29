#include "vulkan_context.hpp"
#include <vector>
#include <cstring>

struct QueueFamilyIndices
{
    uint32_t graphics = UINT32_MAX;
    uint32_t present = UINT32_MAX;

    bool complete() const
    {
        return graphics != UINT32_MAX &&
            present != UINT32_MAX;
    }
};


bool supportsRequiredDeviceExtensions(VkPhysicalDevice device)
{
    const char* requiredExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME
    };

    uint32_t extensionCount = 0;

    if (vkEnumerateDeviceExtensionProperties(
            device,
            nullptr,
            &extensionCount,
            nullptr) != VK_SUCCESS)
    {
        return false;
    }

    std::vector<VkExtensionProperties> extensions(extensionCount);

    if (vkEnumerateDeviceExtensionProperties(
            device,
            nullptr,
            &extensionCount,
            extensions.data()) != VK_SUCCESS)
    {
        return false;
    }

    for (const char* required : requiredExtensions)
    {
        bool found = false;

        for (const auto& extension : extensions)
        {
            if (std::strcmp(
                    extension.extensionName,
                    required) == 0)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            return false;
        }
    }

    return true;
}

QueueFamilyIndices findQueueFamilies(
    VkPhysicalDevice device,
    VkSurfaceKHR surface)
{
    uint32_t count = 0;

    vkGetPhysicalDeviceQueueFamilyProperties(
        device,
        &count,
        nullptr
    );

    std::vector<VkQueueFamilyProperties> families(count);

    vkGetPhysicalDeviceQueueFamilyProperties(
        device,
        &count,
        families.data()
    );

    QueueFamilyIndices indices{};

    for (uint32_t i = 0; i < count; ++i)
    {
        VkBool32 presentSupport = VK_FALSE;

        vkGetPhysicalDeviceSurfaceSupportKHR(
            device,
            i,
            surface,
            &presentSupport
        );

        const bool graphicsSupport =
            (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;

        // Prefer one family capable of both.
        if (graphicsSupport && presentSupport == VK_TRUE)
        {
            indices.graphics = i;
            indices.present = i;
            return indices;
        }

        if (graphicsSupport && indices.graphics == UINT32_MAX)
        {
            indices.graphics = i;
        }

        if (presentSupport == VK_TRUE && indices.present == UINT32_MAX)
        {
            indices.present = i;
        }
    }

    return indices;
}


bool VulkanContext::init(HINSTANCE hInstance, HWND window)
{
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "gsrecon";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "gsrecon";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    const char* instanceExtensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = 2;
    instanceInfo.ppEnabledExtensionNames = instanceExtensions;

    if (vkCreateInstance(
            &instanceInfo,
            nullptr,
            &instance_) != VK_SUCCESS)
    {
        return false;
    }

    VkWin32SurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType =
        VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hinstance = hInstance;
    surfaceInfo.hwnd = window;

    if (vkCreateWin32SurfaceKHR(
            instance_,
            &surfaceInfo,
            nullptr,
            &surface_) != VK_SUCCESS)
    {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;

        return false;
    }

    if (!selectPhysicalDevice())
    {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;

        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;

        return false;
    }

    if (!createLogicalDevice())
    {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;

        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;

        return false;
    }

    return true;
}

bool supportsRequiredFeatures(VkPhysicalDevice device)
{
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = &features13;

    VkPhysicalDeviceFeatures2 features{};
    features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &features12;

    vkGetPhysicalDeviceFeatures2(
        device,
        &features
    );

    return
        features12.timelineSemaphore == VK_TRUE &&
        features13.dynamicRendering == VK_TRUE &&
        features13.synchronization2 == VK_TRUE;
}

bool VulkanContext::selectPhysicalDevice()
{
    uint32_t deviceCount = 0;

    if (vkEnumeratePhysicalDevices(
            instance_,
            &deviceCount,
            nullptr) != VK_SUCCESS ||
        deviceCount == 0)
    {
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);

    if (vkEnumeratePhysicalDevices(
            instance_,
            &deviceCount,
            devices.data()) != VK_SUCCESS)
    {
        return false;
    }

    int bestScore = -1;

    for (VkPhysicalDevice device : devices)
    {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);

        // gsrecon targets Vulkan 1.4.
        if (properties.apiVersion < VK_API_VERSION_1_4)
        {
            continue;
        }

        if (!supportsRequiredFeatures(device))
        {
            continue;
        }

        QueueFamilyIndices queues =
            findQueueFamilies(device, surface_);

        if (!queues.complete())
        {
            continue;
        }

        if (!supportsRequiredDeviceExtensions(device))
        {
            continue;
        }

        uint32_t formatCount = 0;

        if (vkGetPhysicalDeviceSurfaceFormatsKHR(
                device,
                surface_,
                &formatCount,
                nullptr) != VK_SUCCESS ||
            formatCount == 0)
        {
            continue;
        }

        uint32_t presentModeCount = 0;

        if (vkGetPhysicalDeviceSurfacePresentModesKHR(
                device,
                surface_,
                &presentModeCount,
                nullptr) != VK_SUCCESS ||
            presentModeCount == 0)
        {
            continue;
        }

        int score = 0;

        if (properties.deviceType ==
            VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            score = 2;
        }
        else if (properties.deviceType ==
                 VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
        {
            score = 1;
        }

        if (score > bestScore)
        {
            bestScore = score;

            physicalDevice_ = device;
            graphicsQueueFamily_ = queues.graphics;
            presentQueueFamily_ = queues.present;
        }
    }

    if (physicalDevice_ == VK_NULL_HANDLE)
    {
        return false;
    }

    VkPhysicalDeviceIDProperties idProperties{};
    idProperties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;

    VkPhysicalDeviceProperties2 properties{};
    properties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &idProperties;

    vkGetPhysicalDeviceProperties2(
        physicalDevice_,
        &properties
    );

    std::memcpy(
        deviceUUID_.data(),
        idProperties.deviceUUID,
        VK_UUID_SIZE
    );

    return true;
}





bool VulkanContext::createLogicalDevice()
{
    const float queuePriority = 1.0f;

    std::vector<VkDeviceQueueCreateInfo> queueInfos;

    VkDeviceQueueCreateInfo graphicsQueueInfo{};
    graphicsQueueInfo.sType =
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphicsQueueInfo.queueFamilyIndex =
        graphicsQueueFamily_;
    graphicsQueueInfo.queueCount = 1;
    graphicsQueueInfo.pQueuePriorities =
        &queuePriority;

    queueInfos.push_back(graphicsQueueInfo);

    if (presentQueueFamily_ != graphicsQueueFamily_)
    {
        VkDeviceQueueCreateInfo presentQueueInfo{};
        presentQueueInfo.sType =
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        presentQueueInfo.queueFamilyIndex =
            presentQueueFamily_;
        presentQueueInfo.queueCount = 1;
        presentQueueInfo.pQueuePriorities =
            &queuePriority;

        queueInfos.push_back(presentQueueInfo);
    }

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = &features13;
    features12.timelineSemaphore = VK_TRUE;

    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME
    };

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType =
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = &features12;
    deviceInfo.queueCreateInfoCount =
        static_cast<uint32_t>(queueInfos.size());
    deviceInfo.pQueueCreateInfos =
        queueInfos.data();
    deviceInfo.enabledExtensionCount = 3;
    deviceInfo.ppEnabledExtensionNames =
        deviceExtensions;

    if (vkCreateDevice(
            physicalDevice_,
            &deviceInfo,
            nullptr,
            &device_) != VK_SUCCESS)
    {
        return false;
    }

    vkGetDeviceQueue(
        device_,
        graphicsQueueFamily_,
        0,
        &graphicsQueue_
    );

    vkGetDeviceQueue(
        device_,
        presentQueueFamily_,
        0,
        &presentQueue_
    );

    return true;
}


VulkanContext::~VulkanContext()
{
    if (device_ != VK_NULL_HANDLE)
    {
        vkDestroyDevice(device_, nullptr);
    }

    if (surface_ != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(
            instance_,
            surface_,
            nullptr
        );
    }

    if (instance_ != VK_NULL_HANDLE)
    {
        vkDestroyInstance(
            instance_,
            nullptr
        );
    }
}