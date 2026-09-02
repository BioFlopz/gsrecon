#include "vulkan_context.hpp"
#include <vector>
#include <cstring>


namespace
{

constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

bool validationLayerAvailable()
{
    uint32_t layerCount = 0;

    if (vkEnumerateInstanceLayerProperties(
            &layerCount,
            nullptr) != VK_SUCCESS)
    {
        return false;
    }

    std::vector<VkLayerProperties> layers(layerCount);

    if (vkEnumerateInstanceLayerProperties(
            &layerCount,
            layers.data()) != VK_SUCCESS)
    {
        return false;
    }

    for (const auto& layer : layers)
    {
        if (std::strcmp(
                layer.layerName,
                kValidationLayer) == 0)
        {
            return true;
        }
    }

    return false;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void*)
{
    if (callbackData != nullptr &&
        callbackData->pMessage != nullptr)
    {
        OutputDebugStringA("[Vulkan] ");
        OutputDebugStringA(callbackData->pMessage);
        OutputDebugStringA("\n");
    }

    return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT makeDebugMessengerInfo()
{
    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType =
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    info.pfnUserCallback = debugCallback;

    return info;
}

} // namespace



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


bool supportsExternalStorageBuffer(VkPhysicalDevice device)
{
    VkPhysicalDeviceExternalBufferInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO;
    info.flags = 0;
    info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

    VkExternalBufferProperties properties{};
    properties.sType = VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES;

    vkGetPhysicalDeviceExternalBufferProperties(device, &info, &properties);

    const VkExternalMemoryFeatureFlags features = properties.externalMemoryProperties.externalMemoryFeatures;

    return (features & VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT) != 0 && (features & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) == 0;
}

bool supportsExternalSemaphore(VkPhysicalDevice device)
{
    VkPhysicalDeviceExternalSemaphoreInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO;
    info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;

    VkExternalSemaphoreProperties properties{};
    properties.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES;

    vkGetPhysicalDeviceExternalSemaphoreProperties(device, &info, &properties);

    return (properties.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT) != 0;
}


QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    uint32_t count = 0;

    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);

    std::vector<VkQueueFamilyProperties> families(count);

    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    QueueFamilyIndices indices{};

    constexpr VkQueueFlags requiredGraphicsFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;

    for (uint32_t i = 0; i < count; ++i)
    {
        VkBool32 presentSupport = VK_FALSE;

        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        const bool graphicsComputeSupport = (families[i].queueFlags & requiredGraphicsFlags) == requiredGraphicsFlags;

        // Prefer one family capable of graphics, compute, and present.
        if (graphicsComputeSupport && presentSupport == VK_TRUE)
        {
            indices.graphics = i;
            indices.present = i;
            return indices;
        }

        if (graphicsComputeSupport && indices.graphics == UINT32_MAX)
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

    std::vector<const char*> instanceExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };

#ifdef GSRECON_ENABLE_VALIDATION
        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());

    instanceInfo.ppEnabledExtensionNames = instanceExtensions.data();

#ifdef GSRECON_ENABLE_VALIDATION

        if (!validationLayerAvailable())
        {
            return false;
        }

        instanceInfo.enabledLayerCount = 1;
        instanceInfo.ppEnabledLayerNames = &kValidationLayer;

        VkDebugUtilsMessengerCreateInfoEXT debugInfo = makeDebugMessengerInfo();

        instanceInfo.pNext = &debugInfo;

#endif

    if (vkCreateInstance(&instanceInfo, nullptr, &instance_) != VK_SUCCESS)
    {
        return false;
    }

#ifdef GSRECON_ENABLE_VALIDATION

    auto createDebugMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));

    if (createDebugMessenger == nullptr || createDebugMessenger(instance_, &debugInfo, nullptr, &debugMessenger_) != VK_SUCCESS)
    {
        cleanup();
        return false;
    }

#endif

    VkWin32SurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hinstance = hInstance;
    surfaceInfo.hwnd = window;

    if (vkCreateWin32SurfaceKHR(instance_, &surfaceInfo, nullptr, &surface_) != VK_SUCCESS)
    {
        cleanup();
        return false;
    }

    if (!selectPhysicalDevice())
    {
        cleanup();
        return false;
    }

    if (!createLogicalDevice())
    {
        cleanup();
        return false;
    }

    return true;
}

bool supportsRequiredFeatures(VkPhysicalDevice device)
{
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = &features13;

    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &features12;

    vkGetPhysicalDeviceFeatures2(device, &features);

    return
        features12.timelineSemaphore == VK_TRUE &&
        features13.dynamicRendering == VK_TRUE &&
        features13.synchronization2 == VK_TRUE;
}

bool VulkanContext::selectPhysicalDevice()
{
    uint32_t deviceCount = 0;

    if (vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr) != VK_SUCCESS || deviceCount == 0)
    {
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);

    if (vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data()) != VK_SUCCESS)
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

        QueueFamilyIndices queues = findQueueFamilies(device, surface_);

        if (!queues.complete())
        {
            continue;
        }

        if (!supportsRequiredDeviceExtensions(device))
        {
            continue;
        }

        if (!supportsExternalStorageBuffer(device))
        {
            continue;
        }

        if (!supportsExternalSemaphore(device))
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

        if (vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr) != VK_SUCCESS || presentModeCount == 0)
        {
            continue;
        }

        int score = 0;

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            score = 2;
        }
        else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
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
    idProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;

    VkPhysicalDeviceProperties2 properties{};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &idProperties;

    vkGetPhysicalDeviceProperties2(physicalDevice_, &properties);

    std::memcpy(deviceUUID_.data(), idProperties.deviceUUID, VK_UUID_SIZE);

    return true;
}





bool VulkanContext::createLogicalDevice()
{
    const float queuePriority = 1.0f;

    std::vector<VkDeviceQueueCreateInfo> queueInfos;

    VkDeviceQueueCreateInfo graphicsQueueInfo{};
    graphicsQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphicsQueueInfo.queueFamilyIndex = graphicsQueueFamily_;
    graphicsQueueInfo.queueCount = 1;
    graphicsQueueInfo.pQueuePriorities = &queuePriority;

    queueInfos.push_back(graphicsQueueInfo);

    if (presentQueueFamily_ != graphicsQueueFamily_)
    {
        VkDeviceQueueCreateInfo presentQueueInfo{};
        presentQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        presentQueueInfo.queueFamilyIndex = presentQueueFamily_;
        presentQueueInfo.queueCount = 1;
        presentQueueInfo.pQueuePriorities = &queuePriority;

        queueInfos.push_back(presentQueueInfo);
    }

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = &features13;
    features12.timelineSemaphore = VK_TRUE;

    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME
    };

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = &features12;
    deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    deviceInfo.pQueueCreateInfos = queueInfos.data();
    deviceInfo.enabledExtensionCount = 3;
    deviceInfo.ppEnabledExtensionNames = deviceExtensions;

    if (vkCreateDevice(physicalDevice_, &deviceInfo, nullptr, &device_) != VK_SUCCESS)
    {
        return false;
    }

    vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);

    vkGetDeviceQueue(device_, presentQueueFamily_, 0, &presentQueue_);

    return true;
}



void VulkanContext::cleanup()
{
    if (device_ != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(device_);
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    if (surface_ != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);

        surface_ = VK_NULL_HANDLE;
    }

#ifdef GSRECON_ENABLE_VALIDATION

    if (debugMessenger_ != VK_NULL_HANDLE)
    {
        auto destroyDebugMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));

        if (destroyDebugMessenger != nullptr)
        {
            destroyDebugMessenger(instance_, debugMessenger_, nullptr);
        }

        debugMessenger_ = VK_NULL_HANDLE;
    }

#endif

    if (instance_ != VK_NULL_HANDLE)
    {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}


VulkanContext::~VulkanContext()
{
    cleanup();
}
