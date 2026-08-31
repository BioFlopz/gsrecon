#include "shader.hpp"

#include <cstdint>
#include <fstream>
#include <vector>

bool loadShaderModule(const char* path, VkDevice device, VkShaderModule* shaderModule)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        return false;
    }

    const std::streamsize fileSize = file.tellg();

    if (fileSize <= 0 || fileSize % sizeof(std::uint32_t) != 0)
    {
        return false;
    }

    std::vector<std::uint32_t> code(static_cast<std::size_t>(fileSize) / sizeof(std::uint32_t));

    file.seekg(0);

    if (!file.read(reinterpret_cast<char*>(code.data()), fileSize))
    {
        return false;
    }

    VkShaderModuleCreateInfo createInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };

    createInfo.codeSize = static_cast<std::size_t>(fileSize);

    createInfo.pCode = code.data();

    return vkCreateShaderModule(device, &createInfo, nullptr, shaderModule) == VK_SUCCESS;
}


