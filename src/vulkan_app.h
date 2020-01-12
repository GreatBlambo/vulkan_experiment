#ifndef APP_H
#define APP_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

struct VulkanDeviceConfig
{
    std::vector<const char*> validation_layers;
    std::vector<const char*> instance_extensions;
    std::vector<const char*> device_extensions;
    VkPhysicalDeviceFeatures device_features = {};
};

struct VulkanPhysicalDevice
{
    int graphics_family_index = -1;
    int present_family_index = -1;

    VkPhysicalDevice vk_physical_device;
    VkPhysicalDeviceProperties vk_physical_device_props;
    VkPhysicalDeviceFeatures vk_physical_device_features;
    VkPhysicalDeviceMemoryProperties vk_physical_device_mem_props;
    VkSurfaceCapabilitiesKHR vk_surface_capabilities;
    std::vector<VkExtensionProperties> vk_extension_props;
    std::vector<VkQueueFamilyProperties> vk_queue_props;
    std::vector<VkSurfaceFormatKHR> vk_surface_formats;
    std::vector<VkPresentModeKHR> vk_presentation_modes;
};

struct VulkanApp
{
    GLFWwindow* window; 

    VkInstance vk_instance;
    VkDebugUtilsMessengerEXT vk_debug_messenger;
    VkSurfaceKHR vk_surface;

    const VulkanPhysicalDevice* gpu;
    std::vector<VulkanPhysicalDevice> available_gpus;

    VkDevice device;
};

VulkanApp vulkan_init(int width, int height, const char* name,
                      const VulkanDeviceConfig& in_device_config = {},
                      const VkPhysicalDeviceFeatures& in_phys_device_features = {});
void vulkan_deinit(VulkanApp* app);

#endif