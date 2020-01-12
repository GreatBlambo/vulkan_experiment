#include "vulkan_app.h"
#include "utils.h"

int main()
{
    VulkanDeviceConfig device_config;
    #ifdef APP_DEBUG
        device_config.validation_layers = std::vector<const char*>({
            "VK_LAYER_LUNARG_standard_validation"
        });
    #endif
    device_config.instance_extensions = std::vector<const char*>({
        "VK_EXT_debug_utils"
    });

    VulkanApp app = vulkan_init(800, 600, "App", device_config);

    while (!glfwWindowShouldClose(app.window))
    {
        glfwPollEvents();
    }

    vulkan_deinit(&app);

    return 0;
}