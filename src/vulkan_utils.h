#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "utils.h"

#define VK_CHECK(x)                                                                            \
    do                                                                                         \
    {                                                                                          \
        VkResult ret = x;                                                                      \
        ASSERT_MSG(ret == VK_SUCCESS, "VK_CHECK failure: %s - %s", vk_result_string(ret), #x); \
    } while (false);

const char* vk_result_string(VkResult result);