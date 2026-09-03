
extern "C" bool runCudaKernelSmoke();
extern "C" bool runCudaExternalMemorySmoke(void* mappedStorage);
extern "C" bool runCudaExternalGaussianWriteAsync(void* mappedStorage);
extern "C" bool runCudaExternalGaussianSmoke(void* mappedStorage);

#include <Windows.h>
#include <cuda_runtime_api.h>
#include <cstdio>
#include <iostream>
#include <cstring>
#include <vector>
#include <cmath>

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
#include "gaussian.hpp"
#include "camera.hpp"

VkResult acquireSwapchainImage(VkDevice device, const VulkanSwapchain& swapchain, const VulkanFrame& frame, uint32_t& imageIndex)
{
    if (vkWaitForFences(device, 1, &frame.renderFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
    {
        return VK_ERROR_DEVICE_LOST;
    }

    return vkAcquireNextImageKHR(device, swapchain.handle(), UINT64_MAX, frame.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
}




bool recordFrame(VkDevice device, VulkanFrame& frame, const VulkanSwapchain& swapchain, uint32_t imageIndex, VkDescriptorSet gaussianDescriptorSet, VkPipeline gaussianPipeline, VkPipelineLayout gaussianPipelineLayout)
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

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapchain.extent().width);
	viewport.height = static_cast<float>(swapchain.extent().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);
	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = swapchain.extent();
	vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);

	vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gaussianPipeline);
	vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gaussianPipelineLayout, 0, 1, &gaussianDescriptorSet, 0, nullptr);
	vkCmdDraw(frame.commandBuffer, 6, 3, 0, 0);

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


bool submitFrame(VkDevice device, VkQueue graphicsQueue, VulkanFrame& frame, const VulkanSwapchain& swapchain, uint32_t imageIndex, VkSemaphore cudaToVulkanSemaphore, VkSemaphore vulkanToCudaSemaphore)
{
    VkSemaphoreSubmitInfo waitInfos[2]{};

    waitInfos[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitInfos[0].semaphore = frame.imageAvailableSemaphore;
    waitInfos[0].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    waitInfos[1].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitInfos[1].semaphore = cudaToVulkanSemaphore;
    waitInfos[1].stageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;

    VkCommandBufferSubmitInfo commandInfo{};
    commandInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandInfo.commandBuffer = frame.commandBuffer;

    VkSemaphoreSubmitInfo signalInfos[2]{};

    signalInfos[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalInfos[0].semaphore = swapchain.renderFinishedSemaphore(imageIndex);
    signalInfos[0].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    signalInfos[1].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalInfos[1].semaphore = vulkanToCudaSemaphore;
    signalInfos[1].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    VkSubmitInfo2 submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;

    submitInfo.waitSemaphoreInfoCount = 2;
    submitInfo.pWaitSemaphoreInfos = waitInfos;

    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandInfo;

    submitInfo.signalSemaphoreInfoCount = 2;
    submitInfo.pSignalSemaphoreInfos = signalInfos;

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

	if (!selectCudaDeviceForVulkan(vulkan.physicalDevice()))
	{
	    return EXIT_FAILURE;
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

	// if (!selectCudaDeviceForVulkan(vulkan.physicalDevice()))
	// {
	//     return -1;
	// }

	VkDescriptorSetLayoutBinding gaussianStorageBinding{};
	gaussianStorageBinding.binding = 0;
	gaussianStorageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	gaussianStorageBinding.descriptorCount = 1;
	gaussianStorageBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding cameraBinding{};
	cameraBinding.binding = 1;
	cameraBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraBinding.descriptorCount = 1;
	cameraBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding gaussianBindings[] =
	{
	    gaussianStorageBinding,
	    cameraBinding
	};

	VkDescriptorSetLayoutCreateInfo gaussianDescriptorSetLayoutInfo{};
	gaussianDescriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	gaussianDescriptorSetLayoutInfo.bindingCount = 2;
	gaussianDescriptorSetLayoutInfo.pBindings = gaussianBindings;

	VkDescriptorSetLayout gaussianDescriptorSetLayout = VK_NULL_HANDLE;

	if (vkCreateDescriptorSetLayout(vulkan.device(), &gaussianDescriptorSetLayoutInfo, nullptr, &gaussianDescriptorSetLayout) != VK_SUCCESS)
	{
	    return EXIT_FAILURE;
	}

	constexpr uint32_t gaussianCount = 3;
	constexpr VkDeviceSize storageBufferSize = sizeof(GaussianGpuData) * gaussianCount;

	VkBuffer storageBuffer = VK_NULL_HANDLE;

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = storageBufferSize;
	bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkExternalMemoryBufferCreateInfo externalBufferInfo{};
	externalBufferInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
	externalBufferInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

	bufferInfo.pNext = &externalBufferInfo;

	if (vkCreateBuffer(vulkan.device(), &bufferInfo, nullptr, &storageBuffer) != VK_SUCCESS)
	{
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

	    return EXIT_FAILURE;
	}

	if (vkBindBufferMemory(vulkan.device(), storageBuffer, storageMemory, 0) != VK_SUCCESS)
	{
	    vkFreeMemory(vulkan.device(), storageMemory, nullptr);

	    vkDestroyBuffer(vulkan.device(), storageBuffer, nullptr);

	    return EXIT_FAILURE;
	}

	auto getMemoryWin32Handle = reinterpret_cast<PFN_vkGetMemoryWin32HandleKHR>(vkGetDeviceProcAddr(vulkan.device(), "vkGetMemoryWin32HandleKHR"));

	if (getMemoryWin32Handle == nullptr)
	{
	    vkFreeMemory(vulkan.device(), storageMemory, nullptr);
	    vkDestroyBuffer(vulkan.device(), storageBuffer, nullptr);

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

	if (!runCudaExternalGaussianSmoke(cudaMappedStorage))
	{
	    std::cerr << "CUDA Gaussian shared memory smoke: FAILED\n";

	    cudaFree(cudaMappedStorage);
	    cudaDestroyExternalMemory(cudaStorageMemory);

	    return EXIT_FAILURE;
	}

	std::cout << "CUDA Gaussian shared memory smoke: OK\n";

	const VkDeviceSize cameraBufferSize = sizeof(CameraGpuData);

	VkBuffer cameraBuffer = VK_NULL_HANDLE;

	VkBufferCreateInfo cameraBufferInfo{};
	cameraBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	cameraBufferInfo.size = cameraBufferSize;
	cameraBufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	cameraBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(vulkan.device(), &cameraBufferInfo, nullptr, &cameraBuffer) != VK_SUCCESS)
	{
	    std::cerr << "Failed to create camera buffer.\n";
	    return EXIT_FAILURE;
	}

	VkMemoryRequirements cameraMemoryRequirements{};

	vkGetBufferMemoryRequirements(vulkan.device(), cameraBuffer, &cameraMemoryRequirements);

	VkPhysicalDeviceMemoryProperties cameraMemoryProperties{};

	vkGetPhysicalDeviceMemoryProperties(vulkan.physicalDevice(), &cameraMemoryProperties);

	constexpr VkMemoryPropertyFlags cameraRequiredMemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	uint32_t cameraMemoryType = UINT32_MAX;

	for (uint32_t i = 0; i < cameraMemoryProperties.memoryTypeCount; ++i)
	{
	    const bool supportedByBuffer = (cameraMemoryRequirements.memoryTypeBits & (1u << i)) != 0;

	    const bool hasRequiredProperties = (cameraMemoryProperties.memoryTypes[i].propertyFlags & cameraRequiredMemoryProperties) == cameraRequiredMemoryProperties;

	    if (supportedByBuffer && hasRequiredProperties)
	    {
	        cameraMemoryType = i;
	        break;
	    }
	}

	if (cameraMemoryType == UINT32_MAX)
	{
	    vkDestroyBuffer(vulkan.device(), cameraBuffer, nullptr);

	    std::cerr << "No suitable camera memory type.\n";
	    return EXIT_FAILURE;
	}

	VkDeviceMemory cameraMemory = VK_NULL_HANDLE;

	VkMemoryAllocateInfo cameraAllocationInfo{};
	cameraAllocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	cameraAllocationInfo.allocationSize = cameraMemoryRequirements.size;
	cameraAllocationInfo.memoryTypeIndex = cameraMemoryType;

	if (vkAllocateMemory(vulkan.device(), &cameraAllocationInfo, nullptr, &cameraMemory) != VK_SUCCESS)
	{
	    vkDestroyBuffer(vulkan.device(), cameraBuffer, nullptr);

	    std::cerr << "Failed to allocate camera memory.\n";
	    return EXIT_FAILURE;
	}

	if (vkBindBufferMemory(vulkan.device(), cameraBuffer, cameraMemory, 0) != VK_SUCCESS)
	{
	    vkFreeMemory(vulkan.device(), cameraMemory, nullptr);
	    vkDestroyBuffer(vulkan.device(), cameraBuffer, nullptr);

	    std::cerr << "Failed to bind camera memory.\n";
	    return EXIT_FAILURE;
	}

	CameraGpuData initialCamera{};

	initialCamera.view[0]  = 1.0f;
	initialCamera.view[5]  = 1.0f;
	initialCamera.view[10] = 1.0f;
	initialCamera.view[15] = 1.0f;

	// Camera at +0.25 on world X.
	// View matrix therefore moves the world -0.25.
	// initialCamera.view[12] = -0.25f;

	initialCamera.projection[0]  = 1.0f;
	initialCamera.projection[5]  = 1.0f;
	initialCamera.projection[10] = 1.0f;
	initialCamera.projection[15] = 1.0f;

	void* cameraMapped = nullptr;

	if (vkMapMemory(vulkan.device(), cameraMemory, 0, sizeof(CameraGpuData), 0, &cameraMapped) != VK_SUCCESS)
	{
	    vkFreeMemory(vulkan.device(), cameraMemory, nullptr);

	    vkDestroyBuffer(vulkan.device(), cameraBuffer, nullptr);

	    std::cerr << "Failed to map camera memory.\n";
	    return EXIT_FAILURE;
	}

	std::memcpy(cameraMapped, &initialCamera, sizeof(initialCamera));

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

	VkDescriptorPoolSize poolSizes[2]{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = 1;
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = poolSizes;

	if (vkCreateDescriptorPool(vulkan.device(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
	    vkFreeMemory(vulkan.device(), storageMemory, nullptr);
	    vkDestroyBuffer(vulkan.device(), storageBuffer, nullptr);
	    vkDestroyDescriptorSetLayout(vulkan.device(), gaussianDescriptorSetLayout, nullptr);

	    return EXIT_FAILURE;
	}

	VkDescriptorSet gaussianDescriptorSet = VK_NULL_HANDLE;

	VkDescriptorSetAllocateInfo descriptorSetInfo{};
	descriptorSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetInfo.descriptorPool = descriptorPool;
	descriptorSetInfo.descriptorSetCount = 1;
	descriptorSetInfo.pSetLayouts = &gaussianDescriptorSetLayout;

	if (vkAllocateDescriptorSets(vulkan.device(), &descriptorSetInfo, &gaussianDescriptorSet) != VK_SUCCESS)
	{
	    vkDestroyDescriptorPool(vulkan.device(), descriptorPool, nullptr);
	    vkFreeMemory(vulkan.device(), storageMemory, nullptr);
	    vkDestroyBuffer(vulkan.device(), storageBuffer, nullptr);
	    vkDestroyDescriptorSetLayout(vulkan.device(), gaussianDescriptorSetLayout, nullptr);

	    return EXIT_FAILURE;
	}

	VkDescriptorBufferInfo storageBufferInfo{};
	storageBufferInfo.buffer = storageBuffer;
	storageBufferInfo.offset = 0;
	storageBufferInfo.range = storageBufferSize;

	VkWriteDescriptorSet storageWrite{};
	storageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	storageWrite.dstSet = gaussianDescriptorSet;
	storageWrite.dstBinding = 0;
	storageWrite.dstArrayElement = 0;
	storageWrite.descriptorCount = 1;
	storageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	storageWrite.pBufferInfo = &storageBufferInfo;

	vkUpdateDescriptorSets(vulkan.device(), 1, &storageWrite, 0, nullptr);

	VkDescriptorBufferInfo cameraDescriptorInfo{};
	cameraDescriptorInfo.buffer = cameraBuffer;
	cameraDescriptorInfo.offset = 0;
	cameraDescriptorInfo.range = sizeof(CameraGpuData);

	VkWriteDescriptorSet cameraWrite{};
	cameraWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	cameraWrite.dstSet = gaussianDescriptorSet;
	cameraWrite.dstBinding = 1;
	cameraWrite.dstArrayElement = 0;
	cameraWrite.descriptorCount = 1;
	cameraWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraWrite.pBufferInfo = &cameraDescriptorInfo;

	vkUpdateDescriptorSets(vulkan.device(), 1, &cameraWrite, 0, nullptr);


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

	constexpr float pi = 3.14159265358979323846f;

	constexpr float fovYDegrees = 60.0f;
	constexpr float nearPlane = 0.1f;
	constexpr float farPlane = 100.0f;

	const float aspect = static_cast<float>(swapchain.extent().width) / static_cast<float>(swapchain.extent().height);

	const float fovYRadians = fovYDegrees * pi / 180.0f;

	const float yScale = 1.0f / std::tan(fovYRadians * 0.5f);

	const float xScale = yScale / aspect;
	const float zScale = farPlane / (farPlane - nearPlane);
	const float zTranslate = -(nearPlane * farPlane) / (farPlane - nearPlane);


	CameraGpuData camera{};

	//
	// View matrix.
	//
	// Camera position = (0, 0, -1)
	// Camera looks along +Z.
	//
	// Therefore world origin becomes camera-space z = +1.
	//
	camera.view[0]  = 1.0f;
	camera.view[5]  = 1.0f;
	camera.view[10] = 1.0f;
	camera.view[14] = 1.0f;
	camera.view[15] = 1.0f;


	//
	// Perspective projection.
	//
	// Row-vector convention:
	//
	//     clip = viewPosition * projection
	//
	// Produces:
	//     clip.w = viewPosition.z
	//
	// and Vulkan depth in [0, 1].
	//
	camera.projection[0]  = xScale;
	camera.projection[5]  = yScale;

	camera.projection[10] = zScale;
	camera.projection[11] = 1.0f;

	camera.projection[14] = zTranslate;
	camera.projection[15] = 0.0f;


	std::memcpy(cameraMapped, &camera, sizeof(camera));

	VkShaderModule gaussianVertexShaderModule = VK_NULL_HANDLE;

	if (!loadShaderModule(GSRECON_SHADER_DIR "/gaussian.vert.spv", vulkan.device(), &gaussianVertexShaderModule))
	{
	    std::cerr << "Failed to load Gaussian vertex shader.\n";
	    return EXIT_FAILURE;
	}

	VkShaderModule gaussianFragmentShaderModule = VK_NULL_HANDLE;

	if (!loadShaderModule(GSRECON_SHADER_DIR "/gaussian.frag.spv", vulkan.device(), &gaussianFragmentShaderModule))
	{
	    vkDestroyShaderModule(vulkan.device(), gaussianVertexShaderModule, nullptr);

	    std::cerr << "Failed to load Gaussian fragment shader.\n";
	    return EXIT_FAILURE;
	}


	VkPipelineLayout gaussianPipelineLayout = VK_NULL_HANDLE;

	VkPipelineLayoutCreateInfo gaussianPipelineLayoutInfo{};
	gaussianPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	gaussianPipelineLayoutInfo.setLayoutCount = 1;
	gaussianPipelineLayoutInfo.pSetLayouts = &gaussianDescriptorSetLayout;

	if (vkCreatePipelineLayout(vulkan.device(), &gaussianPipelineLayoutInfo, nullptr, &gaussianPipelineLayout) != VK_SUCCESS)
	{
	    vkDestroyShaderModule(vulkan.device(), gaussianFragmentShaderModule, nullptr);
	    vkDestroyShaderModule(vulkan.device(), gaussianVertexShaderModule, nullptr);

	    std::cerr << "Failed to create Gaussian pipeline layout.\n";
	    return EXIT_FAILURE;
	}


	VkPipelineShaderStageCreateInfo gaussianShaderStages[2]{};
	gaussianShaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	gaussianShaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	gaussianShaderStages[0].module = gaussianVertexShaderModule;
	gaussianShaderStages[0].pName = "main";
	gaussianShaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	gaussianShaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	gaussianShaderStages[1].module = gaussianFragmentShaderModule;
	gaussianShaderStages[1].pName = "main";


	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;


	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;


	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;


	VkPipelineRasterizationStateCreateInfo rasterization{};
	rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.depthClampEnable = VK_FALSE;
	rasterization.rasterizerDiscardEnable = VK_FALSE;
	rasterization.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization.cullMode = VK_CULL_MODE_NONE;
	rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterization.depthBiasEnable = VK_FALSE;
	rasterization.lineWidth = 1.0f;


	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.sampleShadingEnable = VK_FALSE;


	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;


	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;


	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;


	VkDynamicState dynamicStates[] =
	{
	    VK_DYNAMIC_STATE_VIEWPORT,
	    VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;


	const VkFormat gaussianColorFormat = swapchain.imageFormat();

	VkPipelineRenderingCreateInfo renderingPipelineInfo{};
	renderingPipelineInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	renderingPipelineInfo.colorAttachmentCount = 1;
	renderingPipelineInfo.pColorAttachmentFormats = &gaussianColorFormat;


	VkGraphicsPipelineCreateInfo gaussianPipelineInfo{};
	gaussianPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gaussianPipelineInfo.pNext = &renderingPipelineInfo;
	gaussianPipelineInfo.stageCount = 2;
	gaussianPipelineInfo.pStages = gaussianShaderStages;
	gaussianPipelineInfo.pVertexInputState = &vertexInputInfo;
	gaussianPipelineInfo.pInputAssemblyState = &inputAssembly;
	gaussianPipelineInfo.pViewportState = &viewportState;
	gaussianPipelineInfo.pRasterizationState = &rasterization;
	gaussianPipelineInfo.pMultisampleState = &multisampling;
	gaussianPipelineInfo.pDepthStencilState = &depthStencil;
	gaussianPipelineInfo.pColorBlendState = &colorBlending;
	gaussianPipelineInfo.pDynamicState = &dynamicState;
	gaussianPipelineInfo.layout = gaussianPipelineLayout;
	gaussianPipelineInfo.renderPass = VK_NULL_HANDLE;


	VkPipeline gaussianPipeline = VK_NULL_HANDLE;

	if (vkCreateGraphicsPipelines(vulkan.device(), VK_NULL_HANDLE, 1, &gaussianPipelineInfo, nullptr, &gaussianPipeline) != VK_SUCCESS)
	{
	    vkDestroyPipelineLayout(vulkan.device(), gaussianPipelineLayout, nullptr);
	    vkDestroyShaderModule(vulkan.device(), gaussianFragmentShaderModule, nullptr);
	    vkDestroyShaderModule(vulkan.device(), gaussianVertexShaderModule, nullptr);

	    std::cerr << "Failed to create Gaussian graphics pipeline.\n";
	    return EXIT_FAILURE;
	}


	vkDestroyShaderModule(vulkan.device(), gaussianFragmentShaderModule, nullptr);
	vkDestroyShaderModule(vulkan.device(), gaussianVertexShaderModule, nullptr);

	std::cout << "Gaussian graphics pipeline: OK\n";

	VulkanFrame frame;

	if (!frame.init(vulkan.device(), vulkan.graphicsQueueFamily()))
	{
	    return -1;
	}

	if (vkQueueSubmit2(vulkan.graphicsQueue(), 1, &externalSignalSubmit, VK_NULL_HANDLE) != VK_SUCCESS)
	{
	    return EXIT_FAILURE;
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

	if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
	{
	    exitCode = -1;
	    running = false;
	    break;
	}

	cudaExternalSemaphoreWaitParams frameCudaWaitParams{};
	frameCudaWaitParams.flags = 0;

	if (cudaWaitExternalSemaphoresAsync(&cudaExternalSemaphore, &frameCudaWaitParams, 1, 0) != cudaSuccess)
	{
	    std::cerr << "Frame CUDA wait: FAILED\n";
	    exitCode = -1;
	    running = false;
	    break;
	}

	if (!runCudaExternalGaussianWriteAsync(cudaMappedStorage))
	{
	    std::cerr << "Frame CUDA shared write: FAILED\n";
	    exitCode = -1;
	    running = false;
	    break;
	}

	cudaExternalSemaphoreSignalParams frameCudaSignalParams{};
	frameCudaSignalParams.flags = 0;

	if (cudaSignalExternalSemaphoresAsync(&cudaToVulkanExternalSemaphore, &frameCudaSignalParams, 1, 0) != cudaSuccess)
	{
	    std::cerr << "Frame CUDA signal: FAILED\n";
	    exitCode = -1;
	    running = false;
	    break;
	}

	if (!recordFrame(vulkan.device(), frame, swapchain, imageIndex, gaussianDescriptorSet, gaussianPipeline, gaussianPipelineLayout))
	{
	    exitCode = -1;
	    running = false;
	    break;
	}

	if (!submitFrame(vulkan.device(), vulkan.graphicsQueue(), frame, swapchain, imageIndex, cudaToVulkanSemaphore, externalSemaphore))
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

	const cudaError_t destroyCudaToVulkanSemaphoreResult = cudaDestroyExternalSemaphore(cudaToVulkanExternalSemaphore);
	const cudaError_t destroyVulkanToCudaSemaphoreResult = cudaDestroyExternalSemaphore(cudaExternalSemaphore);

	vkDestroySemaphore(vulkan.device(), cudaToVulkanSemaphore, nullptr);
	vkDestroySemaphore(vulkan.device(), externalSemaphore, nullptr);

	if (destroyCudaToVulkanSemaphoreResult != cudaSuccess || destroyVulkanToCudaSemaphoreResult != cudaSuccess)
	{
	    return EXIT_FAILURE;
	}

	vkDeviceWaitIdle(vulkan.device());

	bool storageReadbackOk = false;

	void* mappedReadback = nullptr;

	if (vkMapMemory(vulkan.device(), storageMemory, 0, sizeof(GaussianGpuData), 0, &mappedReadback) == VK_SUCCESS)
	{
	    const auto* gaussian = static_cast<const GaussianGpuData*>(mappedReadback);

	    storageReadbackOk =
	        gaussian->position[0] == 0.0f &&
	        gaussian->position[1] == 0.0f &&
	        gaussian->position[2] == 0.0f &&

	        gaussian->opacity == 1.0f &&

	        gaussian->scale[0] == 0.25f &&
	        gaussian->scale[1] == 0.25f &&
	        gaussian->scale[2] == 0.25f &&

	        gaussian->rotation[0] == 1.0f &&
	        gaussian->rotation[1] == 0.0f &&
	        gaussian->rotation[2] == 0.0f &&
	        gaussian->rotation[3] == 0.0f &&

	        gaussian->color[0] == 1.0f &&
	        gaussian->color[1] == 0.0f &&
	        gaussian->color[2] == 0.0f;

	    std::cout
	        << "Gaussian buffer position: "
	        << gaussian->position[0] << ' '
	        << gaussian->position[1] << ' '
	        << gaussian->position[2] << '\n';

	    vkUnmapMemory(vulkan.device(), storageMemory);
	}

	if (cudaFree(cudaMappedStorage) != cudaSuccess)
	{
	    return EXIT_FAILURE;
	}

	if (cudaDestroyExternalMemory(cudaStorageMemory) != cudaSuccess)
	{
	    return EXIT_FAILURE;
	}

	vkDestroyBuffer(vulkan.device(), storageBuffer, nullptr);
	vkFreeMemory(vulkan.device(), storageMemory, nullptr);

	vkUnmapMemory(vulkan.device(), cameraMemory);
	vkDestroyBuffer(vulkan.device(), cameraBuffer, nullptr);
	vkFreeMemory(vulkan.device(), cameraMemory, nullptr);

	vkDestroyPipeline(vulkan.device(), gaussianPipeline, nullptr);
	vkDestroyPipelineLayout(vulkan.device(), gaussianPipelineLayout, nullptr);

	vkDestroyDescriptorPool(vulkan.device(), descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(vulkan.device(), gaussianDescriptorSetLayout, nullptr);

	return storageReadbackOk ? EXIT_SUCCESS : EXIT_FAILURE;
}
