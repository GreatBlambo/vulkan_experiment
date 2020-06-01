#pragma once

#include <GLFW/glfw3.h>

#include "utils.h"

#define VK_CHECK(x)                                                                            \
    do                                                                                         \
    {                                                                                          \
        VkResult ret = x;                                                                      \
        ASSERT_MSG(ret == VK_SUCCESS, "VK_CHECK failure: %s - %s", Vulkan::vk_result_string(ret), #x); \
    } while (false);

namespace Vulkan {

enum class Type {
    FLOAT,
    VEC2,
    VEC3,
    VEC4,
    MAT2,
    MAT3,
    MAT4
};

struct TypeInfo {
    size_t data_size     = 0;
    VkFormat format      = VK_FORMAT_UNDEFINED;
    size_t location_span = 0;
};

TypeInfo get_type_info(Type type);

const char* vk_result_string(VkResult result);

}