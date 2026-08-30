#include "vulkan_swapchain.hpp"

#include <algorithm>

namespace
{

struct SurfaceSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

bool querySurfaceSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, SurfaceSupportDetails& support)
{
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &support.capabilities) != VK_SUCCESS)
    {
        return false;
    }

    uint32_t formatCount = 0;

    if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr) != VK_SUCCESS)
    {
        return false;
    }

    support.formats.resize(formatCount);

    if (formatCount > 0 &&
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, support.formats.data()) != VK_SUCCESS)
    {
        return false;
    }

    uint32_t presentModeCount = 0;

    if (vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr) != VK_SUCCESS)
    {
        return false;
    }

    support.presentModes.resize(presentModeCount);

    if (presentModeCount > 0 &&
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, support.presentModes.data()) != VK_SUCCESS)
    {
        return false;
    }

    return true;
}

VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
    for (const auto& format : formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }

    return formats.front();
}

VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>&)
{
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height)
{
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }

    VkExtent2D extent{width, height};

    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);

    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return extent;
}

uint32_t chooseImageCount(const VkSurfaceCapabilitiesKHR& capabilities)
{
    uint32_t imageCount = capabilities.minImageCount + 1;

    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
    {
        imageCount = capabilities.maxImageCount;
    }

    return imageCount;
}


VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported)
{
    constexpr VkCompositeAlphaFlagBitsKHR preferred[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
    };

    for (VkCompositeAlphaFlagBitsKHR mode : preferred)
    {
        if ((supported & mode) != 0)
        {
            return mode;
        }
    }

    return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
}

} // namespace



bool VulkanSwapchain::init(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
    uint32_t graphicsQueueFamily, uint32_t presentQueueFamily,uint32_t width, uint32_t height)
{
    if (swapchain_ != VK_NULL_HANDLE ||
        physicalDevice == VK_NULL_HANDLE ||
        device == VK_NULL_HANDLE ||
        surface == VK_NULL_HANDLE)
    {
        return false;
    }

    SurfaceSupportDetails support{};

    if (!querySurfaceSupport(physicalDevice, surface, support))
    {
        return false;
    }

    if (support.formats.empty() ||
        support.presentModes.empty())
    {
        return false;
    }

    const VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(support.formats);

    const VkPresentModeKHR presentMode = choosePresentMode(support.presentModes);

    const VkExtent2D extent = chooseExtent(support.capabilities, width, height);

    if (extent.width == 0 ||
        extent.height == 0)
    {
        return false;
    }

    const uint32_t imageCount = chooseImageCount(support.capabilities);

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

    createInfo.surface = surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;

    createInfo.imageArrayLayers = 1;

    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = {
        graphicsQueueFamily,
        presentQueueFamily
    };

    if (graphicsQueueFamily != presentQueueFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;

        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = support.capabilities.currentTransform;

    createInfo.compositeAlpha = chooseCompositeAlpha(support.capabilities.supportedCompositeAlpha);

    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS)
    {
        return false;
    }

	uint32_t swapchainImageCount = 0;

	if (vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr) != VK_SUCCESS || swapchainImageCount == 0)
	{
	    vkDestroySwapchainKHR(device, swapchain, nullptr);
	    return false;
	}

	std::vector<VkImage> images(swapchainImageCount);

	if (vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, images.data()) != VK_SUCCESS)
	{
	    vkDestroySwapchainKHR(device, swapchain, nullptr);
	    return false;
	}

	std::vector<VkImageView> imageViews;
	imageViews.reserve(images.size());

	for (VkImage image : images)
	{
	    VkImageViewCreateInfo viewInfo{};
	    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	    viewInfo.image = image;
	    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	    viewInfo.format = surfaceFormat.format;

	    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	    viewInfo.subresourceRange.baseMipLevel = 0;
	    viewInfo.subresourceRange.levelCount = 1;
	    viewInfo.subresourceRange.baseArrayLayer = 0;
	    viewInfo.subresourceRange.layerCount = 1;

	    VkImageView imageView = VK_NULL_HANDLE;

	    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
	    {
	        for (VkImageView createdView : imageViews)
	        {
	            vkDestroyImageView(device, createdView, nullptr);
	        }

	        vkDestroySwapchainKHR(device, swapchain, nullptr);

	        return false;
	    }

	    imageViews.push_back(imageView);
	}

    std::vector<VkSemaphore> renderFinishedSemaphores;
    renderFinishedSemaphores.reserve(images.size());

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (size_t i = 0; i < images.size(); ++i)
    {
        VkSemaphore semaphore = VK_NULL_HANDLE;

        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
        {
            // Destroy any semaphores we already created.
            for (VkSemaphore createdSemaphore : renderFinishedSemaphores)
            {
                vkDestroySemaphore(device, createdSemaphore, nullptr);
            }

            // Destroy image views created earlier.
            for (VkImageView imageView : imageViews)
            {
                vkDestroyImageView(device, imageView, nullptr);
            }

            // Destroy the temporary swapchain.
            vkDestroySwapchainKHR(device, swapchain, nullptr);

            return false;
        }

        renderFinishedSemaphores.push_back(semaphore);
    }

	device_ = device;
	swapchain_ = swapchain;

	images_ = std::move(images);
	imageViews_ = std::move(imageViews);

    renderFinishedSemaphores_ = std::move(renderFinishedSemaphores);

	imageFormat_ = surfaceFormat.format;
	extent_ = extent;

	return true;
}


void VulkanSwapchain::cleanup()
{
    if (device_ == VK_NULL_HANDLE)
    {
        return;
    }

    for (VkImageView imageView : imageViews_)
    {
        vkDestroyImageView(device_, imageView, nullptr);
    }

    imageViews_.clear();
    images_.clear();

    for (VkSemaphore semaphore :
         renderFinishedSemaphores_)
    {
        vkDestroySemaphore(device_, semaphore, nullptr);
    }

    renderFinishedSemaphores_.clear();

    if (swapchain_ != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);

        swapchain_ = VK_NULL_HANDLE;
    }

    imageFormat_ = VK_FORMAT_UNDEFINED;
    extent_ = {};
    device_ = VK_NULL_HANDLE;
}



VulkanSwapchain::~VulkanSwapchain()
{
    cleanup();
}