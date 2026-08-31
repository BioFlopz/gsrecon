#pragma once

#include <vulkan/vulkan.h>

bool loadShaderModule(const char* path, VkDevice device, VkShaderModule* shaderModule);

