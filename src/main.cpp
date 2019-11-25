#include "vulkan_app.h"
#include "utils.h"

static const char* validation_layers[] = { 
    "VK_LAYER_LUNARG_standard_validation" 
};

static const char* extensions[] = { 
    "VK_EXT_debug_utils" 
};

int main()
{
    VulkanExtensionInfo extension_info;
    #ifdef APP_DEBUG
        extension_info.validation_layers = validation_layers;
        extension_info.num_validation_layers = ARRAY_LENGTH(validation_layers);
    #endif
    extension_info.extensions = extensions;
    extension_info.num_extensions = ARRAY_LENGTH(extensions);

    VulkanApp app = vulkan_init(800, 600, "App", &extension_info);

    while (!glfwWindowShouldClose(app.window))
    {
        glfwPollEvents();
    }

    vulkan_deinit(&app);

    return 0;
}