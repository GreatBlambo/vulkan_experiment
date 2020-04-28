#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <unordered_map>

#include "handle.h"

#define VULKAN_DEFAULT_FRAMES_IN_FLIGHT 2
#define VULKAN_DEFAULT_RENDERING_FRAMES VULKAN_DEFAULT_FRAMES_IN_FLIGHT

namespace Vulkan
{

struct DeviceConfig
{
    std::vector< const char* > validation_layers;
    std::vector< const char* > instance_extensions;
    std::vector< const char* > device_extensions;
    VkPhysicalDeviceFeatures   device_features      = {};
    unsigned int               max_frames_in_flight = 0;
    unsigned int               max_rendering_frames = 0;
};

struct PhysicalDevice
{
    int graphics_family_index = -1;
    int present_family_index  = -1;

    VkPhysicalDevice                       vk_physical_device;
    VkPhysicalDeviceProperties             vk_physical_device_props;
    VkPhysicalDeviceFeatures               vk_physical_device_features;
    VkPhysicalDeviceMemoryProperties       vk_physical_device_mem_props;
    VkSurfaceCapabilitiesKHR               vk_surface_capabilities;
    std::vector< VkExtensionProperties >   vk_extension_props;
    std::vector< VkQueueFamilyProperties > vk_queue_props;
    std::vector< VkSurfaceFormatKHR >      vk_surface_formats;
    std::vector< VkPresentModeKHR >        vk_presentation_modes;
};

struct SwapchainImage
{
    VkImage     image;
    VkImageView image_view;
};

struct FrameResources
{
    VkCommandBuffer command_buffer;
    VkFence         draw_complete_fence;
    VkSemaphore     acquire_semaphore;
    VkSemaphore     draw_complete_semaphore;
};

struct App
{
    App(int width, int height, const char* name, const DeviceConfig& in_device_config = {});
    ~App();

    GLFWwindow* window;
    int         width;
    int         height;

    VkInstance               vk_instance;
    VkDebugUtilsMessengerEXT vk_debug_messenger;
    VkSurfaceKHR             vk_surface;

    size_t                        gpu_index;
    std::vector< PhysicalDevice > available_gpus;

    VkDevice device;
    VkQueue  graphics_queue;
    VkQueue  present_queue;

    // Swapchain resources for each frame
    // size = max_frames_in_flight
    unsigned int                  max_frames_in_flight;
    VkSwapchainKHR                swapchain;
    VkFormat                      swapchain_format;
    VkExtent2D                    swapchain_extent;
    std::vector< SwapchainImage > swapchain_images;

    // Primary resources for each frame
    // There's a fence for each one so that the next
    // frame which uses it waits until commands are
    // recorded to begin presentation.
    //
    // IMPORTANT: This fence is not necessarily
    // in sync with the completion of presentation.
    // We're decoupling rendering and presentation,
    // so that command buffers can be reused and
    // recorded on while presentation is still
    // occurring. The command buffers can be
    // submitted while presentation for the acquired
    // swapchain image is occurring, but the command
    // queue submit is blocked by a semaphore (GPU
    // resident synchronization) which is signalled
    // on presentation complete.
    //
    // In other words, just because a swapchain
    // image is acquired, doesn't mean it has
    // completed presentation.
    //
    // size = max_rendering_frames
    unsigned int                  max_rendering_frames;
    unsigned int                  current_frame = 0;
    VkCommandPool                 command_pool;
    std::vector< FrameResources > frame_resources;

    bool     image_format_supported(VkFormat format, VkFormatFeatureFlags required,
                                    VkImageTiling tiling);
    VkFormat default_depth_stencil_format();

    // Resources
};

}    // namespace Vulkan