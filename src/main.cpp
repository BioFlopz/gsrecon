
#include <Windows.h>
#include <vulkan/vulkan.h>
#include "vulkan_context.hpp"
#include "vulkan_swapchain.hpp"

#include <vector>

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}



int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    int nCmdShow)
{
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

	if (!swapchain.init(
	        vulkan.physicalDevice(),
	        vulkan.device(),
	        vulkan.surface(),
	        vulkan.graphicsQueueFamily(),
	        vulkan.presentQueueFamily(),
	        width,
	        height))
	{
	    return -1;
	}



	MSG message{};

	while (GetMessage(&message, nullptr, 0, 0) > 0)
	{
	    TranslateMessage(&message);
	    DispatchMessage(&message);
	}

	return 0;
}
