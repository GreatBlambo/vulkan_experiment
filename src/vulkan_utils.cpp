#include <glm/glm.hpp>

#include "vulkan_utils.h"

#define VK_ERROR_STRING_CASE(x) case static_cast< int >( x ): return #x

namespace Vulkan {

TypeInfo get_type_info(Type type) {
    switch (type) {
        case Type::FLOAT: return { sizeof(float), VK_FORMAT_R32_SFLOAT, 1 };
        case Type::VEC2: return { sizeof(glm::vec2), VK_FORMAT_R32G32_SFLOAT, 1 };
        case Type::VEC3: return { sizeof(glm::vec3), VK_FORMAT_R32G32B32_SFLOAT, 1 };
        case Type::VEC4:
            return { sizeof(glm::vec4), VK_FORMAT_R32G32B32A32_SFLOAT, 1 };
        case Type::MAT2: return { sizeof(glm::mat2), VK_FORMAT_R32G32_SFLOAT, 2 };
        case Type::MAT3: return { sizeof(glm::mat3), VK_FORMAT_R32G32B32_SFLOAT, 3 };
        case Type::MAT4:
            return { sizeof(glm::mat4), VK_FORMAT_R32G32B32A32_SFLOAT, 4 };
        default: return { 0, VK_FORMAT_UNDEFINED, 0 };
    }
}

const char* vk_result_string(VkResult result)
{
    switch (result)
    {
        VK_ERROR_STRING_CASE(VK_SUCCESS);
        VK_ERROR_STRING_CASE(VK_NOT_READY);
        VK_ERROR_STRING_CASE(VK_TIMEOUT);
        VK_ERROR_STRING_CASE(VK_EVENT_SET);
        VK_ERROR_STRING_CASE(VK_EVENT_RESET);
        VK_ERROR_STRING_CASE(VK_INCOMPLETE);
        VK_ERROR_STRING_CASE(VK_ERROR_OUT_OF_HOST_MEMORY);
        VK_ERROR_STRING_CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY);
        VK_ERROR_STRING_CASE(VK_ERROR_INITIALIZATION_FAILED);
        VK_ERROR_STRING_CASE(VK_ERROR_DEVICE_LOST);
        VK_ERROR_STRING_CASE(VK_ERROR_MEMORY_MAP_FAILED);
        VK_ERROR_STRING_CASE(VK_ERROR_LAYER_NOT_PRESENT);
        VK_ERROR_STRING_CASE(VK_ERROR_EXTENSION_NOT_PRESENT);
        VK_ERROR_STRING_CASE(VK_ERROR_FEATURE_NOT_PRESENT);
        VK_ERROR_STRING_CASE(VK_ERROR_INCOMPATIBLE_DRIVER);
        VK_ERROR_STRING_CASE(VK_ERROR_TOO_MANY_OBJECTS);
        VK_ERROR_STRING_CASE(VK_ERROR_FORMAT_NOT_SUPPORTED);
        VK_ERROR_STRING_CASE(VK_ERROR_SURFACE_LOST_KHR);
        VK_ERROR_STRING_CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
        VK_ERROR_STRING_CASE(VK_SUBOPTIMAL_KHR);
        VK_ERROR_STRING_CASE(VK_ERROR_OUT_OF_DATE_KHR);
        VK_ERROR_STRING_CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
        VK_ERROR_STRING_CASE(VK_ERROR_VALIDATION_FAILED_EXT);
        VK_ERROR_STRING_CASE(VK_ERROR_INVALID_SHADER_NV);
        VK_ERROR_STRING_CASE(VK_RESULT_BEGIN_RANGE);
        VK_ERROR_STRING_CASE(VK_RESULT_RANGE_SIZE);
        default: return "UNKNOWN";
    }
}
}