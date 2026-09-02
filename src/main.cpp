
extern "C" bool runCudaKernelSmoke();
extern "C" bool runCudaExternalMemorySmoke(void* mappedStorage);

#include <Windows.h>
#include <cuda_runtime_api.h>
#include <cstdio>
#include <iostream>
#include <cstring>
#include <vector>


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
#include "shader.hpp"



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




bool recordClearFrame(VkDevice device, VulkanFrame& frame, const VulkanSwapchain& swapchain, uint32_t imageIndex, VkPipeline computePipeline, VkPipelineLayout computePipelineLayout, VkDescriptorSet computeDescriptorSet)
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

	vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);

	vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSet, 0, nullptr);

	vkCmdDispatch(frame.commandBuffer, 1, 1, 1);

	VkMemoryBarrier2 storageReadbackBarrier{};
	storageReadbackBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
	storageReadbackBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	storageReadbackBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
	storageReadbackBarrier.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
	storageReadbackBarrier.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;

	VkDependencyInfo storageReadbackDependency{};
	storageReadbackDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	storageReadbackDependency.memoryBarrierCount = 1;
	storageReadbackDependency.pMemoryBarriers = &storageReadbackBarrier;

	vkCmdPipelineBarrier2(frame.commandBuffer, &storageReadbackDependency);

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



bool selectCudaDeviceForVulkan(VkPhysicalDevice physicalDevice)
{
    VkPhysicalDeviceIDProperties idProperties{};
    idProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;

    VkPhysicalDeviceProperties2 properties{};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &idProperties;

    vkGetPhysicalDeviceProperties2(physicalDevice, &properties);

    int deviceCount = 0;

    if (cudaGetDeviceCount(&deviceCount) != cudaSuccess || deviceCount == 0)
    {
        return false;
    }

    for (int device = 0; device < deviceCount; ++device)
    {
        cudaDeviceProp cudaProperties{};

        if (cudaGetDeviceProperties(&cudaProperties, device) != cudaSuccess)
        {
            continue;
        }

        if (std::memcmp(&cudaProperties.uuid, idProperties.deviceUUID, VK_UUID_SIZE) != 0)
        {
            continue;
        }

        if (cudaSetDevice(device) != cudaSuccess)
        {
            return false;
        }

		if (!runCudaKernelSmoke())
		{
		    std::cerr << "CUDA kernel: FAILED\n";
		    return false;
		}

		std::cout << "CUDA kernel: OK\n";

#ifdef GSRECON_ENABLE_DEBUG_CONSOLE
        std::printf("CUDA device matched Vulkan: %s\n", cudaProperties.name);
#endif

        return true;
    }

    return false;
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

	VkExportSemaphoreCreateInfo exportSemaphoreInfo{};
	exportSemaphoreInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
	exportSemaphoreInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreInfo.pNext = &exportSemaphoreInfo;

	VkSemaphore externalSemaphore = VK_NULL_HANDLE;

	if (vkCreateSemaphore(vulkan.device(), &semaphoreInfo, nullptr, &externalSemaphore) != VK_SUCCESS)
	{
	    return EXIT_FAILURE;
	}

	std::cout << "Vulkan external semaphore: OK\n";

	auto getSemaphoreWin32Handle = reinterpret_cast<PFN_vkGetSemaphoreWin32HandleKHR>(vkGetDeviceProcAddr(vulkan.device(), "vkGetSemaphoreWin32HandleKHR"));

	if (getSemaphoreWin32Handle == nullptr)
	{
	    vkDestroySemaphore(vulkan.device(), externalSemaphore, nullptr);

	    return EXIT_FAILURE;
	}

	VkSemaphoreGetWin32HandleInfoKHR semaphoreHandleInfo{};
	semaphoreHandleInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
	semaphoreHandleInfo.semaphore = externalSemaphore;
	semaphoreHandleInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;

	HANDLE externalSemaphoreHandle = nullptr;

	if (getSemaphoreWin32Handle(vulkan.device(), &semaphoreHandleInfo, &externalSemaphoreHandle) != VK_SUCCESS)
	{
	    vkDestroySemaphore(vulkan.device(), externalSemaphore, nullptr);

	    return EXIT_FAILURE;
	}

	std::cout << "Vulkan external semaphore handle: OK\n";

	cudaExternalSemaphoreHandleDesc cudaSemaphoreHandleInfo{};
	cudaSemaphoreHandleInfo.type = cudaExternalSemaphoreHandleTypeOpaqueWin32;
	cudaSemaphoreHandleInfo.handle.win32.handle = externalSemaphoreHandle;
	cudaSemaphoreHandleInfo.flags = 0;

	cudaExternalSemaphore_t cudaExternalSemaphore = nullptr;

	const cudaError_t semaphoreImportResult = cudaImportExternalSemaphore(&cudaExternalSemaphore, &cudaSemaphoreHandleInfo);

	// CUDA does not take ownership of the Win32 NT handle.
	CloseHandle(externalSemaphoreHandle);
	externalSemaphoreHandle = nullptr;

	if (semaphoreImportResult != cudaSuccess)
	{
	    std::cerr << "CUDA external semaphore import failed: " << cudaGetErrorString(semaphoreImportResult) << '\n';

	    vkDestroySemaphore(vulkan.device(), externalSemaphore, nullptr);

	    return EXIT_FAILURE;
	}

	std::cout << "CUDA external semaphore import: OK\n";

	VkSemaphoreSubmitInfo externalSignalInfo{};
	externalSignalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	externalSignalInfo.semaphore = externalSemaphore;
	externalSignalInfo.value = 0;
	externalSignalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

	VkSubmitInfo2 externalSignalSubmit{};
	externalSignalSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	externalSignalSubmit.signalSemaphoreInfoCount = 1;
	externalSignalSubmit.pSignalSemaphoreInfos = &externalSignalInfo;

	if (vkQueueSubmit2(vulkan.graphicsQueue(), 1, &externalSignalSubmit, VK_NULL_HANDLE) != VK_SUCCESS)
	{
	    cudaDestroyExternalSemaphore(cudaExternalSemaphore);
	    vkDestroySemaphore(vulkan.device(), externalSemaphore, nullptr);

	    return EXIT_FAILURE;
	}

	cudaExternalSemaphoreWaitParams cudaWaitParams{};
	cudaWaitParams.flags = 0;

	const cudaError_t cudaWaitResult = cudaWaitExternalSemaphoresAsync(&cudaExternalSemaphore, &cudaWaitParams, 1, 0);

	if (cudaWaitResult != cudaSuccess || cudaDeviceSynchronize() != cudaSuccess)
	{
	    std::cerr << "CUDA external semaphore wait: FAILED\n";

	    vkQueueWaitIdle(vulkan.graphicsQueue());

	    cudaDestroyExternalSemaphore(cudaExternalSemaphore);
	    vkDestroySemaphore(vulkan.device(), externalSemaphore, nullptr);

	    return EXIT_FAILURE;
	}

	std::cout << "Vulkan -> CUDA semaphore sync: OK\n";

	VkSemaphore cudaToVulkanSemaphore = VK_NULL_HANDLE;

	if (vkCreateSemaphore(vulkan.device(), &semaphoreInfo, nullptr, &cudaToVulkanSemaphore) != VK_SUCCESS)
	{
	    cudaDestroyExternalSemaphore(cudaExternalSemaphore);

	    vkDestroySemaphore(vulkan.device(), externalSemaphore, nullptr);

	    return EXIT_FAILURE;
	}

	std::cout << "Vulkan CUDA -> Vulkan semaphore: OK\n";

	VkSemaphoreGetWin32HandleInfoKHR cudaToVulkanHandleInfo{};
	cudaToVulkanHandleInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
	cudaToVulkanHandleInfo.semaphore = cudaToVulkanSemaphore;
	cudaToVulkanHandleInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;

	HANDLE cudaToVulkanHandle = nullptr;

	if (getSemaphoreWin32Handle(vulkan.device(), &cudaToVulkanHandleInfo, &cudaToVulkanHandle) != VK_SUCCESS)
	{
	    vkDestroySemaphore(vulkan.device(), cudaToVulkanSemaphore, nullptr);

	    cudaDestroyExternalSemaphore(cudaExternalSemaphore);

	    vkDestroySemaphore(vulkan.device(), externalSemaphore, nullptr);

	    return EXIT_FAILURE;
	}

	std::cout << "Vulkan CUDA -> Vulkan semaphore handle: OK\n";

	cudaExternalSemaphoreHandleDesc cudaToVulkanHandleDesc{};
	cudaToVulkanHandleDesc.type = cudaExternalSemaphoreHandleTypeOpaqueWin32;
	cudaToVulkanHandleDesc.handle.win32.handle = cudaToVulkanHandle;
	cudaToVulkanHandleDesc.flags = 0;

	cudaExternalSemaphore_t cudaToVulkanExternalSemaphore = nullptr;

	const cudaError_t cudaToVulkanImportResult = cudaImportExternalSemaphore(&cudaToVulkanExternalSemaphore, &cudaToVulkanHandleDesc);

	CloseHandle(cudaToVulkanHandle);
	cudaToVulkanHandle = nullptr;

	if (cudaToVulkanImportResult != cudaSuccess)
	{
	    std::cerr << "CUDA -> Vulkan semaphore import failed: " << cudaGetErrorString(cudaToVulkanImportResult) << '\n';

	    vkDestroySemaphore(vulkan.device(), cudaToVulkanSemaphore, nullptr);

	    cudaDestroyExternalSemaphore(cudaExternalSemaphore);

	    vkDestroySemaphore(vulkan.device(), externalSemaphore, nullptr);

	    return EXIT_FAILURE;
	}

	std::cout << "CUDA -> Vulkan semaphore import: OK\n";

	cudaExternalSemaphoreSignalParams cudaSignalParams{};
	cudaSignalParams.flags = 0;

	const cudaError_t cudaSignalResult = cudaSignalExternalSemaphoresAsync(&cudaToVulkanExternalSemaphore, &cudaSignalParams, 1, 0);

	if (cudaSignalResult != cudaSuccess)
	{
	    std::cerr << "CUDA -> Vulkan semaphore signal: FAILED\n";

	    cudaDestroyExternalSemaphore(cudaToVulkanExternalSemaphore);

	    vkDestroySemaphore(vulkan.device(), cudaToVulkanSemaphore, nullptr);

	    return EXIT_FAILURE;
	}

	VkSemaphoreSubmitInfo cudaToVulkanWaitInfo{};
	cudaToVulkanWaitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	cudaToVulkanWaitInfo.semaphore = cudaToVulkanSemaphore;
	cudaToVulkanWaitInfo.value = 0;
	cudaToVulkanWaitInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

	VkSubmitInfo2 cudaToVulkanWaitSubmit{};
	cudaToVulkanWaitSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	cudaToVulkanWaitSubmit.waitSemaphoreInfoCount = 1;
	cudaToVulkanWaitSubmit.pWaitSemaphoreInfos = &cudaToVulkanWaitInfo;

	if (vkQueueSubmit2(vulkan.graphicsQueue(), 1, &cudaToVulkanWaitSubmit, VK_NULL_HANDLE) != VK_SUCCESS)
	{
	    cudaDestroyExternalSemaphore(cudaToVulkanExternalSemaphore);

	    vkDestroySemaphore(vulkan.device(), cudaToVulkanSemaphore, nullptr);

	    return EXIT_FAILURE;
	}

	if (vkQueueWaitIdle(vulkan.graphicsQueue()) != VK_SUCCESS)
	{
	    cudaDestroyExternalSemaphore(cudaToVulkanExternalSemaphore);

	    vkDestroySemaphore(vulkan.device(), cudaToVulkanSemaphore, nullptr);

	    return EXIT_FAILURE;
	}

	std::cout << "CUDA -> Vulkan semaphore sync: OK\n";

	cudaDestroyExternalSemaphore(cudaToVulkanExternalSemaphore);

	vkDestroySemaphore(vulkan.device(), cudaToVulkanSemaphore, nullptr);

	vkQueueWaitIdle(vulkan.graphicsQueue());

	if (cudaDestroyExternalSemaphore(cudaExternalSemaphore) != cudaSuccess)
	{
	    vkDestroySemaphore(vulkan.device(), externalSemaphore, nullptr);

	    return EXIT_FAILURE;
	}

	vkDestroySemaphore(vulkan.device(), externalSemaphore, nullptr);

	if (!selectCudaDeviceForVulkan(vulkan.physicalDevice()))
	{
	    return -1;
	}

	VkShaderModule computeShaderModule = VK_NULL_HANDLE;

	if (!loadShaderModule(GSRECON_SHADER_DIR "/compute.spv", vulkan.device(), &computeShaderModule))
	{
#ifdef GSRECON_ENABLE_DEBUG_CONSOLE
    std::fprintf(stderr, "Failed to create compute shader module.\n");
#endif

	    return -1;
	}

#ifdef GSRECON_ENABLE_DEBUG_CONSOLE
	std::printf("Compute shader module: OK\n");
#endif

	VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE;

	VkDescriptorSetLayoutBinding storageBinding{};
	storageBinding.binding = 0;
	storageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	storageBinding.descriptorCount = 1;
	storageBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
	descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorLayoutInfo.bindingCount = 1;
	descriptorLayoutInfo.pBindings = &storageBinding;

	if (vkCreateDescriptorSetLayout(vulkan.device(), &descriptorLayoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS)
	{
	    return EXIT_FAILURE;
	}

	VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &computeDescriptorSetLayout;

	if (vkCreatePipelineLayout(vulkan.device(), &layoutInfo, nullptr, &computePipelineLayout) != VK_SUCCESS)
	{
	    vkDestroyShaderModule(vulkan.device(), computeShaderModule, nullptr);

	    return -1;
	}

	VkPipelineShaderStageCreateInfo stageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };

	stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageInfo.module = computeShaderModule;
	stageInfo.pName = "main";

	VkComputePipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };

	pipelineInfo.stage = stageInfo;
	pipelineInfo.layout = computePipelineLayout;

	VkPipeline computePipeline = VK_NULL_HANDLE;

	if (vkCreateComputePipelines(vulkan.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS)
	{
	    vkDestroyPipelineLayout(vulkan.device(), computePipelineLayout, nullptr);

	    vkDestroyShaderModule(vulkan.device(), computeShaderModule, nullptr);

	    return -1;
	}

	VkBuffer storageBuffer = VK_NULL_HANDLE;

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(uint32_t) * 4;
	bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkExternalMemoryBufferCreateInfo externalBufferInfo{};
	externalBufferInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
	externalBufferInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

	bufferInfo.pNext = &externalBufferInfo;

	if (vkCreateBuffer(vulkan.device(), &bufferInfo, nullptr, &storageBuffer) != VK_SUCCESS)
	{
	    vkDestroyPipeline(vulkan.device(), computePipeline, nullptr);

	    vkDestroyPipelineLayout(vulkan.device(), computePipelineLayout, nullptr);

	    return EXIT_FAILURE;
	}

	VkMemoryRequirements storageMemoryRequirements{};

	vkGetBufferMemoryRequirements(vulkan.device(), storageBuffer, &storageMemoryRequirements);

	VkPhysicalDeviceMemoryProperties memoryProperties{};

	vkGetPhysicalDeviceMemoryProperties(vulkan.physicalDevice(), &memoryProperties);

	constexpr VkMemoryPropertyFlags requiredMemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	uint32_t storageMemoryType = UINT32_MAX;

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
	{
	    const bool supportedByBuffer = (storageMemoryRequirements.memoryTypeBits & (1u << i)) != 0;

	    const bool hasRequiredProperties = (memoryProperties.memoryTypes[i].propertyFlags & requiredMemoryProperties) == requiredMemoryProperties;

	    if (supportedByBuffer && hasRequiredProperties)
	    {
	        storageMemoryType = i;
	        break;
	    }
	}

	if (storageMemoryType == UINT32_MAX)
	{
	    vkDestroyBuffer(vulkan.device(), storageBuffer, nullptr);

	    vkDestroyPipeline(vulkan.device(), computePipeline, nullptr);

	    vkDestroyPipelineLayout(vulkan.device(), computePipelineLayout, nullptr
	    );

	    return EXIT_FAILURE;
	}

	VkDeviceMemory storageMemory = VK_NULL_HANDLE;

	VkExportMemoryAllocateInfo exportInfo{};
	exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
	exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

	VkMemoryAllocateInfo allocationInfo{};
	allocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocationInfo.pNext = &exportInfo;
	allocationInfo.allocationSize = storageMemoryRequirements.size;
	allocationInfo.memoryTypeIndex = storageMemoryType;

	if (vkAllocateMemory(vulkan.device(), &allocationInfo, nullptr, &storageMemory) != VK_SUCCESS)
	{
	    vkDestroyBuffer(vulkan.device(), storageBuffer, nullptr);

	    vkDestroyPipeline(vulkan.device(), computePipeline, nullptr);

	    vkDestroyPipelineLayout(vulkan.device(), computePipelineLayout, nullptr);

	    return EXIT_FAILURE;
	}

	if (vkBindBufferMemory(vulkan.device(), storageBuffer, storageMemory, 0) != VK_SUCCESS)
	{
	    vkFreeMemory(vulkan.device(), storageMemory, nullptr);

	    vkDestroyBuffer(vulkan.device(), storageBuffer, nullptr);

	    vkDestroyPipeline(vulkan.device(), computePipeline, nullptr);

	    vkDestroyPipelineLayout(vulkan.device(), computePipelineLayout, nullptr);

	    return EXIT_FAILURE;
	}

	auto getMemoryWin32Handle = reinterpret_cast<PFN_vkGetMemoryWin32HandleKHR>(vkGetDeviceProcAddr(vulkan.device(), "vkGetMemoryWin32HandleKHR"));

	if (getMemoryWin32Handle == nullptr)
	{
	    vkFreeMemory(vulkan.device(), storageMemory, nullptr);
	    vkDestroyBuffer(vulkan.device(), storageBuffer, nullptr);

	    vkDestroyPipeline(vulkan.device(), computePipeline, nullptr);
	    vkDestroyPipelineLayout(vulkan.device(), computePipelineLayout, nullptr);

	    return EXIT_FAILURE;
	}

	VkMemoryGetWin32HandleInfoKHR handleInfo{};
	handleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
	handleInfo.memory = storageMemory;
	handleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

	HANDLE storageMemoryHandle = nullptr;

	if (getMemoryWin32Handle(vulkan.device(), &handleInfo, &storageMemoryHandle) != VK_SUCCESS)
	{
	    vkFreeMemory(vulkan.device(), storageMemory, nullptr);
	    vkDestroyBuffer(vulkan.device(), storageBuffer, nullptr);

	    vkDestroyPipeline(vulkan.device(), computePipeline, nullptr);
	    vkDestroyPipelineLayout(vulkan.device(), computePipelineLayout, nullptr);

	    return EXIT_FAILURE;
	}

	std::cout << "Vulkan external memory handle: OK\n";

	cudaExternalMemoryHandleDesc cudaHandleInfo{};
	cudaHandleInfo.type = cudaExternalMemoryHandleTypeOpaqueWin32;
	cudaHandleInfo.handle.win32.handle = storageMemoryHandle;
	cudaHandleInfo.size = static_cast<unsigned long long>(storageMemoryRequirements.size);
	cudaHandleInfo.flags = 0;

	cudaExternalMemory_t cudaStorageMemory = nullptr;

	const cudaError_t importResult = cudaImportExternalMemory(&cudaStorageMemory, &cudaHandleInfo);

	// CUDA does not take ownership of an OPAQUE_WIN32 NT handle.
	CloseHandle(storageMemoryHandle);
	storageMemoryHandle = nullptr;

	if (importResult != cudaSuccess)
	{
	    vkFreeMemory(vulkan.device(), storageMemory, nullptr);
	    vkDestroyBuffer(vulkan.device(), storageBuffer, nullptr);

	    vkDestroyPipeline(vulkan.device(), computePipeline, nullptr);
	    vkDestroyPipelineLayout(vulkan.device(), computePipelineLayout, nullptr);

	    return EXIT_FAILURE;
	}

	std::cout << "CUDA external memory import: OK\n";

	cudaExternalMemoryBufferDesc cudaBufferInfo{};
	cudaBufferInfo.offset = 0;
	cudaBufferInfo.size = static_cast<unsigned long long>(storageMemoryRequirements.size);
	cudaBufferInfo.flags = 0;

	void* cudaMappedStorage = nullptr;

	const cudaError_t mapResult = cudaExternalMemoryGetMappedBuffer(&cudaMappedStorage, cudaStorageMemory, &cudaBufferInfo);

	if (mapResult != cudaSuccess)
	{
	    std::cerr << "CUDA external memory map failed: " << cudaGetErrorString(mapResult) << '\n';

	    cudaDestroyExternalMemory(cudaStorageMemory);

	    vkFreeMemory(vulkan.device(), storageMemory, nullptr);
	    vkDestroyBuffer(vulkan.device(), storageBuffer, nullptr);

	    vkDestroyPipeline(vulkan.device(), computePipeline, nullptr);
	    vkDestroyPipelineLayout(vulkan.device(), computePipelineLayout, nullptr);

	    return EXIT_FAILURE;
	}

	std::cout << "CUDA external memory map: OK\n";

	if (!runCudaExternalMemorySmoke(cudaMappedStorage))
	{
	    std::cerr << "CUDA external memory smoke: FAILED\n";

	    cudaFree(cudaMappedStorage);
	    cudaDestroyExternalMemory(cudaStorageMemory);

	    return EXIT_FAILURE;
	}

	std::cout << "CUDA external memory smoke: OK\n";

	if (cudaFree(cudaMappedStorage) != cudaSuccess)
	{
	    cudaDestroyExternalMemory(cudaStorageMemory);

	    vkFreeMemory(vulkan.device(), storageMemory, nullptr);
	    vkDestroyBuffer(vulkan.device(), storageBuffer, nullptr);

	    vkDestroyPipeline(vulkan.device(), computePipeline, nullptr);
	    vkDestroyPipelineLayout(vulkan.device(), computePipelineLayout, nullptr);

	    return EXIT_FAILURE;
	}

	if (cudaDestroyExternalMemory(cudaStorageMemory) != cudaSuccess)
	{
	    vkFreeMemory(vulkan.device(), storageMemory, nullptr);
	    vkDestroyBuffer(vulkan.device(), storageBuffer, nullptr);

	    vkDestroyPipeline(vulkan.device(), computePipeline, nullptr);
	    vkDestroyPipelineLayout(vulkan.device(), computePipelineLayout, nullptr);

	    return EXIT_FAILURE;
	}

	void* mappedStorageData = nullptr;

	if (vkMapMemory(vulkan.device(), storageMemory, 0, sizeof(uint32_t) * 4, 0, &mappedStorageData) != VK_SUCCESS)
	{
	    vkFreeMemory(vulkan.device(), storageMemory, nullptr);

	    vkDestroyBuffer(vulkan.device(), storageBuffer, nullptr);

	    vkDestroyPipeline(vulkan.device(), computePipeline, nullptr);

	    vkDestroyPipelineLayout(vulkan.device(), computePipelineLayout, nullptr);

	    return EXIT_FAILURE;
	}

	auto* storageValues = static_cast<uint32_t*>(mappedStorageData);

	storageValues[0] = 0;
	storageValues[1] = 0;
	storageValues[2] = 0;
	storageValues[3] = 0;

	vkUnmapMemory(vulkan.device(), storageMemory);

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = 1;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;

	if (vkCreateDescriptorPool(vulkan.device(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
	    vkFreeMemory(vulkan.device(), storageMemory, nullptr);

	    vkDestroyBuffer(vulkan.device(), storageBuffer, nullptr);

	    vkDestroyPipeline(vulkan.device(), computePipeline, nullptr);

	    vkDestroyPipelineLayout(vulkan.device(), computePipelineLayout, nullptr);

	    vkDestroyDescriptorSetLayout(vulkan.device(), computeDescriptorSetLayout, nullptr);

	    return EXIT_FAILURE;
	}

	VkDescriptorSet computeDescriptorSet = VK_NULL_HANDLE;

	VkDescriptorSetAllocateInfo descriptorSetInfo{};
	descriptorSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetInfo.descriptorPool = descriptorPool;
	descriptorSetInfo.descriptorSetCount = 1;
	descriptorSetInfo.pSetLayouts = &computeDescriptorSetLayout;

	if (vkAllocateDescriptorSets(vulkan.device(), &descriptorSetInfo, &computeDescriptorSet) != VK_SUCCESS)
	{
	    vkDestroyDescriptorPool(vulkan.device(), descriptorPool, nullptr);

	    vkFreeMemory(vulkan.device(), storageMemory, nullptr);

	    vkDestroyBuffer(vulkan.device(), storageBuffer, nullptr);

	    vkDestroyPipeline(vulkan.device(), computePipeline, nullptr);

	    vkDestroyPipelineLayout(vulkan.device(), computePipelineLayout, nullptr);

	    vkDestroyDescriptorSetLayout(vulkan.device(), computeDescriptorSetLayout, nullptr);

	    return EXIT_FAILURE;
	}

	VkDescriptorBufferInfo storageBufferInfo{};
	storageBufferInfo.buffer = storageBuffer;
	storageBufferInfo.offset = 0;
	storageBufferInfo.range = sizeof(uint32_t) * 4;

	VkWriteDescriptorSet storageWrite{};
	storageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	storageWrite.dstSet = computeDescriptorSet;
	storageWrite.dstBinding = 0;
	storageWrite.dstArrayElement = 0;
	storageWrite.descriptorCount = 1;
	storageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	storageWrite.pBufferInfo = &storageBufferInfo;

	vkUpdateDescriptorSets(vulkan.device(), 1, &storageWrite, 0, nullptr);

	if (storageMemoryType == UINT32_MAX)
	{
	    vkDestroyBuffer(vulkan.device(), storageBuffer, nullptr);

	    vkDestroyPipeline(vulkan.device(), computePipeline, nullptr);

	    vkDestroyPipelineLayout(vulkan.device(), computePipelineLayout, nullptr);

	    return EXIT_FAILURE;
	}

#ifdef GSRECON_ENABLE_DEBUG_CONSOLE
	std::printf("Compute pipeline: OK\n");
#endif

	vkDestroyShaderModule(vulkan.device(), computeShaderModule, nullptr);

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

	if (!recordClearFrame(vulkan.device(), frame, swapchain, imageIndex, computePipeline, computePipelineLayout, computeDescriptorSet))
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

	vkDeviceWaitIdle(vulkan.device());

	bool storageReadbackOk = false;

	void* mappedReadback = nullptr;

	if (vkMapMemory(vulkan.device(), storageMemory, 0, sizeof(uint32_t) * 4, 0, &mappedReadback) == VK_SUCCESS)
	{
	    const auto* values = static_cast<const uint32_t*>(mappedReadback);

	    storageReadbackOk =
	        values[0] == 1 &&
	        values[1] == 2 &&
	        values[2] == 3 &&
	        values[3] == 4;

	    std::cout
	        << "Storage buffer: "
	        << values[0] << ' '
	        << values[1] << ' '
	        << values[2] << ' '
	        << values[3] << '\n';

	    vkUnmapMemory(vulkan.device(), storageMemory);
	}

	vkDestroyBuffer(vulkan.device(), storageBuffer, nullptr);

	vkFreeMemory(vulkan.device(), storageMemory, nullptr);

	vkDestroyPipeline(vulkan.device(), computePipeline, nullptr);

	vkDestroyPipelineLayout(vulkan.device(), computePipelineLayout, nullptr);

	vkDestroyDescriptorPool(vulkan.device(), descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(vulkan.device(), computeDescriptorSetLayout, nullptr);

	return storageReadbackOk ? EXIT_SUCCESS : EXIT_FAILURE;
}
