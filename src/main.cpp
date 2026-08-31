

#include <Windows.h>
#include <cstdio>


#ifdef GSRECON_ENABLE_DEBUG_CONSOLE
void attachDebugConsole()
{
    if (!AllocConsole())
    {
        return;
    }

    FILE* stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
}
#endif


#include <vulkan/vulkan.h>
#include "vulkan_context.hpp"
#include "vulkan_swapchain.hpp"
#include "vulkan_frame.hpp"

#include <vector>


VkResult acquireSwapchainImage(
    VkDevice device,
    const VulkanSwapchain& swapchain,
    const VulkanFrame& frame,
    uint32_t& imageIndex)
{
    if (vkWaitForFences(device, 1, &frame.renderFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
    {
        return VK_ERROR_DEVICE_LOST;
    }

    return vkAcquireNextImageKHR(device, swapchain.handle(), UINT64_MAX, frame.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
}




bool recordClearFrame(VkDevice device, VulkanFrame& frame, const VulkanSwapchain& swapchain, uint32_t imageIndex)
{
    if (imageIndex >= swapchain.images().size() ||
        imageIndex >= swapchain.imageViews().size())
    {
        return false;
    }

    if (vkResetCommandPool(device, frame.commandPool, 0) != VK_SUCCESS)
    {
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(frame.commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        return false;
    }

    VkImageMemoryBarrier2 toColor{};
    toColor.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    toColor.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    toColor.srcAccessMask = VK_ACCESS_2_NONE;
    toColor.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    toColor.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

    toColor.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toColor.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    toColor.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColor.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    toColor.image = swapchain.images()[imageIndex];

    toColor.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toColor.subresourceRange.baseMipLevel = 0;
    toColor.subresourceRange.levelCount = 1;
    toColor.subresourceRange.baseArrayLayer = 0;
    toColor.subresourceRange.layerCount = 1;

    VkDependencyInfo toColorDependency{};
    toColorDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    toColorDependency.imageMemoryBarrierCount = 1;
    toColorDependency.pImageMemoryBarriers = &toColor;

    vkCmdPipelineBarrier2(frame.commandBuffer, &toColorDependency);

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = swapchain.imageViews()[imageIndex];
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    colorAttachment.clearValue.color.float32[0] = 0.05f;
    colorAttachment.clearValue.color.float32[1] = 0.10f;
    colorAttachment.clearValue.color.float32[2] = 0.20f;
    colorAttachment.clearValue.color.float32[3] = 1.0f;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = swapchain.extent();
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(frame.commandBuffer, &renderingInfo);

    vkCmdEndRendering(frame.commandBuffer);

    VkImageMemoryBarrier2 toPresent{};
    toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    toPresent.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    toPresent.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    toPresent.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
    toPresent.dstAccessMask = VK_ACCESS_2_NONE;

    toPresent.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    toPresent.image = swapchain.images()[imageIndex];

    toPresent.subresourceRange = toColor.subresourceRange;

    VkDependencyInfo toPresentDependency{};
    toPresentDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    toPresentDependency.imageMemoryBarrierCount = 1;
    toPresentDependency.pImageMemoryBarriers = &toPresent;

    vkCmdPipelineBarrier2(frame.commandBuffer, &toPresentDependency);

    return vkEndCommandBuffer(frame.commandBuffer) == VK_SUCCESS;
}


bool submitClearFrame(VkDevice device, VkQueue graphicsQueue, VulkanFrame& frame, const VulkanSwapchain& swapchain, uint32_t imageIndex)
{
    VkSemaphoreSubmitInfo waitInfo{};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitInfo.semaphore = frame.imageAvailableSemaphore;
    waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkCommandBufferSubmitInfo commandInfo{};
    commandInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandInfo.commandBuffer = frame.commandBuffer;

    VkSemaphoreSubmitInfo signalInfo{};
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalInfo.semaphore = swapchain.renderFinishedSemaphore(imageIndex);
    signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    VkSubmitInfo2 submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;

    submitInfo.waitSemaphoreInfoCount = 1;
    submitInfo.pWaitSemaphoreInfos = &waitInfo;

    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandInfo;

    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.pSignalSemaphoreInfos = &signalInfo;

    if (vkResetFences(device, 1, &frame.renderFence) != VK_SUCCESS)
    {
        return false;
    }

    return vkQueueSubmit2(graphicsQueue, 1, &submitInfo, frame.renderFence) == VK_SUCCESS;
}


VkResult presentSwapchainImage(VkQueue presentQueue, const VulkanSwapchain& swapchain, uint32_t imageIndex)
{
    VkSwapchainKHR swapchainHandle = swapchain.handle();

	VkSemaphore renderFinished = swapchain.renderFinishedSemaphore(imageIndex);

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinished;

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchainHandle;

    presentInfo.pImageIndices = &imageIndex;


    return vkQueuePresentKHR(presentQueue, &presentInfo);
}



LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
	    case WM_DESTROY:
	        PostQuitMessage(0);
	        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}



int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
#ifdef GSRECON_ENABLE_DEBUG_CONSOLE
    attachDebugConsole();
#endif
    const wchar_t CLASS_NAME[] = L"gsrecon window";

    WNDCLASS windowClass{};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.lpszClassName = CLASS_NAME;

    RegisterClass(&windowClass);

	HWND window = CreateWindowEx(
	    0,
	    CLASS_NAME,
	    L"gsrecon",
	    WS_OVERLAPPEDWINDOW,
	    CW_USEDEFAULT,
	    CW_USEDEFAULT,
	    CW_USEDEFAULT,
	    CW_USEDEFAULT,
	    nullptr,
	    nullptr,
	    hInstance,
	    nullptr
	);

	if (window == nullptr)
	{
	    return 0;
	}

	ShowWindow(window, nCmdShow);

	VulkanContext vulkan;

	if (!vulkan.init(hInstance, window))
	{
	    return 0;
	}

	RECT clientRect{};

	if (!GetClientRect(window, &clientRect))
	{
	    return -1;
	}

	const uint32_t width = static_cast<uint32_t>(clientRect.right - clientRect.left);

	const uint32_t height = static_cast<uint32_t>(clientRect.bottom - clientRect.top);
	
	VulkanSwapchain swapchain;

	if (!swapchain.init(vulkan.physicalDevice(), vulkan.device(), vulkan.surface(), vulkan.graphicsQueueFamily(), vulkan.presentQueueFamily(), width, height))
	{
	    return -1;
	}

	VulkanFrame frame;

	if (!frame.init(vulkan.device(), vulkan.graphicsQueueFamily()))
	{
	    return -1;
	}



	MSG message{};
	bool running = true;
	int exitCode = 0;

	while (running)
	{
	    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
	    {
	        if (message.message == WM_QUIT)
	        {
	            running = false;
	            break;
	        }

	        TranslateMessage(&message);
	        DispatchMessage(&message);
	    }

	    if (!running)
	    {
	        break;
	    }

	uint32_t imageIndex = 0;

	const VkResult acquireResult = acquireSwapchainImage(vulkan.device(), swapchain, frame, imageIndex);

	if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
	{
	    exitCode = -1;
	    running = false;
	    break;
	}

	if (acquireResult != VK_SUCCESS &&
	    acquireResult != VK_SUBOPTIMAL_KHR)
	{
	    exitCode = -1;
	    running = false;
	    break;
	}

	if (!recordClearFrame(vulkan.device(), frame, swapchain, imageIndex))
	{
	    exitCode = -1;
	    running = false;
	    break;
	}

	if (!submitClearFrame(vulkan.device(), vulkan.graphicsQueue(), frame, swapchain, imageIndex))
	{
	    exitCode = -1;
	    running = false;
	    break;
	}

	const VkResult presentResult = presentSwapchainImage(vulkan.presentQueue(), swapchain, imageIndex);

	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR ||
	    presentResult == VK_SUBOPTIMAL_KHR ||
	    acquireResult == VK_SUBOPTIMAL_KHR)
	{
	    running = false;
	    break;
	}

	if (presentResult != VK_SUCCESS)
	{
	    exitCode = -1;
	    running = false;
	    break;
	}
	}

	if (vkDeviceWaitIdle(vulkan.device()) != VK_SUCCESS)
	{
	    return -1;
	}

	return static_cast<int>(message.wParam);
}
