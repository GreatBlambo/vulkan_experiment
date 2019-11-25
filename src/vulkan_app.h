#ifndef APP_H
#define APP_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct VulkanApp
{
    GLFWwindow* window; 
    VkInstance vk_instance;
    VkDebugUtilsMessengerEXT vk_debug_messenger;
    VkPhysicalDevice vk_physical_device;
};

struct VulkanExtensionInfo
{
    const char** validation_layers;
    size_t num_validation_layers = 0;

    const char** extensions;
    size_t num_extensions = 0;
};

VulkanApp vulkan_init(int width, int height, const char* name, const VulkanExtensionInfo* extension_info = NULL);
void vulkan_deinit(VulkanApp* app);

#endif