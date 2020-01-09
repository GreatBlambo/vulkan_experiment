#include "vulkan_app.h"
#include "utils.h"

int main()
{
    VulkanExtensionInfo extension_info;
    #ifdef APP_DEBUG
        extension_info.validation_layers = std::vector<const char*>({
            "VK_LAYER_LUNARG_standard_validation"
        });
    #endif
    extension_info.instance_extensions = std::vector<const char*>({
        "VK_EXT_debug_utils"
    });

    VulkanApp app = vulkan_init(800, 600, "App", &extension_info);

    while (!glfwWindowShouldClose(app.window))
    {
        glfwPollEvents();
    }

    vulkan_deinit(&app);

    return 0;
}