#include "vulkan_app.h"
#include "vulkan_utils.h"
#include "utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <array>
#include <algorithm>

static VkResult vkCreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pCallback) {
    static PFN_vkCreateDebugUtilsMessengerEXT func
        = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
            instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pCallback);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static void vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback,
                                            const VkAllocationCallbacks* pAllocator) {
    static PFN_vkDestroyDebugUtilsMessengerEXT func
        = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
            instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, callback, pAllocator);
    }
}

namespace Vulkan {

static VKAPI_ATTR VkBool32 VKAPI_CALL
                           debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
                                          VkDebugUtilsMessageTypeFlagsEXT             message_type,
                                          const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data, void* p_user_data) {
    const char* prefix = "Validation layer: %s";
    if (message_type == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
        prefix = "Performance issue from validation layer: %s";

    if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
        LOG_DEBUG(prefix, p_callback_data->pMessage);
    if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        LOG_INFO(prefix, p_callback_data->pMessage);
    if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        LOG_WARNING(prefix, p_callback_data->pMessage);
    if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        LOG_ERROR(prefix, p_callback_data->pMessage);

    return VK_FALSE;
}

static void enable_validation_layers(VkInstanceCreateInfo& create_info,
                                     const DeviceConfig&   device_config) {
    LOG_DEBUG("Enabling validation layers");
    // Check for validation layers
    if (device_config.validation_layers.size() <= 0) {
        create_info.enabledLayerCount = 0;
        return;
    }

    uint32_t vl_count = 0;
    vkEnumerateInstanceLayerProperties(&vl_count, NULL);

    VkLayerProperties* supported_layers
        = (VkLayerProperties*) malloc(sizeof(VkLayerProperties) * vl_count);
    vkEnumerateInstanceLayerProperties(&vl_count, supported_layers);

    for (size_t i = 0; i < device_config.validation_layers.size(); i++) {
        bool supported = false;
        for (size_t j = 0; j < vl_count; j++) {
            if (strcmp(supported_layers[j].layerName, device_config.validation_layers[i]) == 0) {
                supported = true;
                break;
            }
        }

        if (!supported) {
            fprintf(stderr, "Error: validation layer %s is not supported\n",
                    device_config.validation_layers[i]);
            exit(1);
        }
    }

    create_info.enabledLayerCount   = device_config.validation_layers.size();
    create_info.ppEnabledLayerNames = device_config.validation_layers.data();
    free(supported_layers);
}

static void add_extensions(VkInstanceCreateInfo& create_info, DeviceConfig& device_config) {
    // Get extensions
    uint32_t     req_extension_count = 0;
    const char** req_extensions      = glfwGetRequiredInstanceExtensions(&req_extension_count);

    for (uint32_t i = 0; i < req_extension_count; i++) {
        device_config.instance_extensions.push_back(req_extensions[i]);
    }

    create_info.enabledExtensionCount   = device_config.instance_extensions.size();
    create_info.ppEnabledExtensionNames = device_config.instance_extensions.data();
}

static VkInstance create_instance(const char* name, DeviceConfig& device_config) {
    LOG_DEBUG("Creating instance");
    // Create application info
    VkApplicationInfo app_info  = {};
    app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName   = name;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName        = "None";
    app_info.engineVersion      = 1;
    app_info.apiVersion         = VK_MAKE_VERSION(1, 0, VK_HEADER_VERSION);

    // Create instance info
    VkInstanceCreateInfo create_info = {};
    create_info.sType                = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo     = &app_info;

    // Enable validation layers
    create_info.enabledLayerCount = 0;
    enable_validation_layers(create_info, device_config);

    // Add extensions
    add_extensions(create_info, device_config);

    // Create instance
    VkInstance instance;
    if (vkCreateInstance(&create_info, NULL, &instance) != VK_SUCCESS) {
        RUNTIME_ERROR("Failed to create instance");
    }

    return instance;
}

static VkDebugUtilsMessengerEXT create_debug_messenger(VkInstance                   instance,
                                                       const VkAllocationCallbacks* allocator) {
#ifdef APP_DEBUG
    LOG_DEBUG("Creating debug messenger");
    // Create debug callback object
    VkDebugUtilsMessengerCreateInfoEXT error_cb_create_info = {};
    error_cb_create_info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    error_cb_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                                           | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                           | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    error_cb_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                                       | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                                       | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    error_cb_create_info.pfnUserCallback = debug_callback;
    error_cb_create_info.pUserData       = NULL;

    VkDebugUtilsMessengerEXT messenger;
    VkResult                 status
        = vkCreateDebugUtilsMessengerEXT(instance, &error_cb_create_info, allocator, &messenger);
    assert(status != VK_ERROR_EXTENSION_NOT_PRESENT);

    return messenger;
#else
    return VK_NULL_HANDLE;
#endif
}

static void find_queue_indices(PhysicalDevice& device, VkSurfaceKHR surface) {
    // Find graphics queue
    for (int i = 0; i < device.vk_queue_props.size(); i++) {
        const VkQueueFamilyProperties& props = device.vk_queue_props[i];

        if (props.queueCount == 0) {
            continue;
        }

        if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            device.graphics_family_index = i;
            break;
        }
    }

    // Find present queue
    for (int i = 0; i < device.vk_queue_props.size(); i++) {
        const VkQueueFamilyProperties& props = device.vk_queue_props[i];

        if (props.queueCount == 0) {
            continue;
        }

        VkBool32 supports_present = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device.vk_physical_device, i, surface,
                                             &supports_present);
        if (supports_present) {
            device.present_family_index = i;
            break;
        }
    }
}

static const size_t pick_physical_device(VkInstance instance, const DeviceConfig& device_config,
                                         VkSurfaceKHR                   surface,
                                         std::vector< PhysicalDevice >& out_gpus) {
    LOG_DEBUG("Picking physical device");
    // Populate physical device information
    uint32_t device_count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &device_count, NULL));

    ASSERT(device_count > 0);

    std::vector< VkPhysicalDevice > devices(device_count);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &device_count, devices.data()));

    out_gpus.resize(device_count);
    for (size_t i = 0; i < device_count; i++) {
        PhysicalDevice& gpu = out_gpus[i];

        gpu.vk_physical_device = devices[i];
        // Get physical device properties
        vkGetPhysicalDeviceProperties(gpu.vk_physical_device, &gpu.vk_physical_device_props);

        // Get physical device vk_physical_device_features
        vkGetPhysicalDeviceFeatures(gpu.vk_physical_device, &gpu.vk_physical_device_features);

        // Get physical device memory properties
        vkGetPhysicalDeviceMemoryProperties(gpu.vk_physical_device,
                                            &gpu.vk_physical_device_mem_props);

        // Get supported device extensions
        {
            uint32_t num_extensions;
            VK_CHECK(vkEnumerateDeviceExtensionProperties(gpu.vk_physical_device, NULL,
                                                          &num_extensions, NULL));
            ASSERT_MSG(num_extensions > 0,
                       "vkEnumerateDeviceExtensionProperties returned zero "
                       "extensions.");

            gpu.vk_extension_props.resize(num_extensions);
            VK_CHECK(vkEnumerateDeviceExtensionProperties(
                gpu.vk_physical_device, NULL, &num_extensions, gpu.vk_extension_props.data()));
        }

        // Get device queues
        {
            uint32_t num_queues;
            vkGetPhysicalDeviceQueueFamilyProperties(gpu.vk_physical_device, &num_queues, NULL);
            ASSERT_MSG(num_queues > 0,
                       "vkGetPhysicalDeviceQueueFamilyProperties returned zero "
                       "queue properties.");

            gpu.vk_queue_props.resize(num_queues);
            vkGetPhysicalDeviceQueueFamilyProperties(gpu.vk_physical_device, &num_queues,
                                                     gpu.vk_queue_props.data());
        }

        // Get surface capabilities
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu.vk_physical_device, surface,
                                                           &gpu.vk_surface_capabilities));

        // Get supported surface formats
        {
            uint32_t num_formats;
            VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.vk_physical_device, surface,
                                                          &num_formats, NULL));
            ASSERT_MSG(num_formats > 0,
                       "vkGetPhysicalDeviceSurfaceFormatsKHR returned zero "
                       "surface formats.");

            gpu.vk_surface_formats.resize(num_formats);
            VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
                gpu.vk_physical_device, surface, &num_formats, gpu.vk_surface_formats.data()));
        }

        // Get supported presentation modes
        {
            uint32_t num_presentation_modes;
            VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.vk_physical_device, surface,
                                                               &num_presentation_modes, NULL));
            ASSERT_MSG(num_presentation_modes > 0,
                       "vkGetPhysicalDeviceSurfacePresentModesKHR returned "
                       "zero presentation modes.");

            gpu.vk_presentation_modes.resize(num_presentation_modes);
            VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.vk_physical_device, surface,
                                                               &num_presentation_modes,
                                                               gpu.vk_presentation_modes.data()));
        }

        LOG_DEBUG("Physical device detected: %s", gpu.vk_physical_device_props.deviceName);
    }

    // Pick physical device
    uint32_t        max_score         = 0;
    size_t          best_device_index = 0;
    PhysicalDevice* best_device       = NULL;
    for (size_t i = 0; i < out_gpus.size(); i++) {
        PhysicalDevice& gpu = out_gpus[i];
        // Check for required vk_physical_device_features
        {
            // Check extensions supported by device
            std::string unsupported_extensions = "";
            for (const char* extension : device_config.device_extensions) {
                auto compare = [&extension](const VkExtensionProperties& extension_prop) {
                    return strcmp(extension_prop.extensionName, extension) == 0;
                };

                if (std::find_if(gpu.vk_extension_props.begin(), gpu.vk_extension_props.end(),
                                 compare)
                    == gpu.vk_extension_props.end()) {
                    unsupported_extensions += extension;
                    unsupported_extensions += ", ";
                }
            }

            if (unsupported_extensions.length() > 0) {
                LOG_DEBUG("Skipping %s since it does not support extensions %s",
                          gpu.vk_physical_device_props.deviceName, unsupported_extensions.data());
                continue;
            }
        }

        if (gpu.vk_surface_formats.size() == 0) {
            LOG_DEBUG("Skipping %s since it does not have any suface formats.",
                      gpu.vk_physical_device_props.deviceName);
            continue;
        }

        if (gpu.vk_presentation_modes.size() == 0) {
            LOG_DEBUG("Skipping %s since it does not have any present modes.",
                      gpu.vk_physical_device_props.deviceName);
            continue;
        }

        find_queue_indices(gpu, surface);

        if (gpu.graphics_family_index == -1) {
            LOG_DEBUG(
                "Skipping %s since it does not have a queue family that "
                "supports graphics.",
                gpu.vk_physical_device_props.deviceName);
            continue;
        }

        if (gpu.present_family_index == -1) {
            LOG_DEBUG(
                "Skipping %s since it does not have a queue family that "
                "supports present.",
                gpu.vk_physical_device_props.deviceName);
            continue;
        }

        // Score device
        uint32_t score = 0;

        struct {
            uint32_t discrete_gpu     = 1 << 10;
            uint32_t geometry_shaders = 1;
        } support_priorities;

        if (gpu.vk_physical_device_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += support_priorities.discrete_gpu;
        }

        if (gpu.vk_physical_device_features.geometryShader) {
            score += support_priorities.geometry_shaders;
        }

        // Select device if it scores the best so far
        if (score >= max_score) {
            max_score         = score;
            best_device_index = i;
            best_device       = &gpu;
        }
    }

    ASSERT_MSG(best_device != NULL, "Device not chosen");

    LOG_DEBUG("Physical device %s chosen.", best_device->vk_physical_device_props.deviceName);

    return best_device_index;
}

static VkSurfaceKHR create_surface(VkInstance instance, GLFWwindow* window) {
    LOG_DEBUG("Creating surface");
    VkSurfaceKHR surface;
    VK_CHECK(glfwCreateWindowSurface(instance, window, NULL, &surface));
    return surface;
}

/*
    phys_device          - physical device to build logical device from
    device_config        - extensions to enable for logical device
   by the supplied physical device
*/
static VkDevice create_logical_device(const PhysicalDevice& phys_device,
                                      const DeviceConfig&   device_config) {
    LOG_DEBUG("Creating logical device");
    const std::array< int, 2 > queue_indices
        = { phys_device.graphics_family_index, phys_device.present_family_index };

    std::vector< VkDeviceQueueCreateInfo > queue_create_infos;
    const float                            priority = 1.0f;
    for (size_t i = 0; i < queue_indices.size(); i++) {
        VkDeviceQueueCreateInfo create_info = {};
        create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        create_info.queueFamilyIndex        = queue_indices[i];
        create_info.queueCount              = 1;
        create_info.pQueuePriorities        = &priority;
        queue_create_infos.push_back(create_info);
    }

    VkDeviceCreateInfo device_create_info      = {};
    device_create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount    = queue_create_infos.size();
    device_create_info.pQueueCreateInfos       = queue_create_infos.data();
    device_create_info.pEnabledFeatures        = &device_config.device_features;
    device_create_info.enabledExtensionCount   = device_config.device_extensions.size();
    device_create_info.ppEnabledExtensionNames = device_config.device_extensions.data();

#ifdef APP_DEBUG
    device_create_info.enabledLayerCount   = device_config.validation_layers.size();
    device_create_info.ppEnabledLayerNames = device_config.validation_layers.data();
#else
    device_create_info.enabledLayerCount = 0;
#endif

    VkDevice logical_device;
    VK_CHECK(
        vkCreateDevice(phys_device.vk_physical_device, &device_create_info, NULL, &logical_device));

    return logical_device;
}

static VkSurfaceFormatKHR pick_surface_format(std::vector< VkSurfaceFormatKHR >& surface_formats) {
    VkSurfaceFormatKHR result;

    if (surface_formats.size() == 1 && surface_formats[0].format == VK_FORMAT_UNDEFINED) {
        result.format     = VK_FORMAT_B8G8R8A8_UNORM;
        result.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        return result;
    }

    for (size_t i = 0; i < surface_formats.size(); i++) {
        VkSurfaceFormatKHR& format = surface_formats[i];
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM
            && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return surface_formats[0];
}

static VkPresentModeKHR pick_present_mode(std::vector< VkPresentModeKHR >& present_modes) {
    const VkPresentModeKHR desired_mode = VK_PRESENT_MODE_MAILBOX_KHR;

    for (int i = 0; i < present_modes.size(); i++) {
        if (present_modes[i] == desired_mode) {
            return desired_mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D pick_surface_extent(App& app, VkSurfaceCapabilitiesKHR& surface_capabilities) {
    VkExtent2D extent;

    if (surface_capabilities.currentExtent.width == -1) {
        extent.width  = app.width;
        extent.height = app.height;
    } else {
        extent = surface_capabilities.currentExtent;
    }

    return extent;
}

static void create_swapchain(App& app) {

    LOG_DEBUG("Creating swapchain");
    PhysicalDevice& gpu = app.available_gpus[app.gpu_index];

    VkSurfaceFormatKHR surface_format = pick_surface_format(gpu.vk_surface_formats);
    VkPresentModeKHR   present_mode   = pick_present_mode(gpu.vk_presentation_modes);
    VkExtent2D         surface_extent = pick_surface_extent(app, gpu.vk_surface_capabilities);

    // Create swapchain
    VkSwapchainCreateInfoKHR info = {};
    info.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface                  = app.vk_surface;
    info.minImageCount            = app.max_frames_in_flight;
    info.imageFormat              = surface_format.format;
    info.imageColorSpace          = surface_format.colorSpace;
    info.imageExtent              = surface_extent;
    info.imageArrayLayers         = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    LOG_DEBUG("Graphics family index = %i, present family index = %i", gpu.graphics_family_index,
              gpu.present_family_index);
    if (gpu.graphics_family_index != gpu.present_family_index) {
        uint32_t indices[]
            = { (uint32_t) gpu.graphics_family_index, (uint32_t) gpu.present_family_index };
        info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices   = indices;
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    info.preTransform   = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode    = present_mode;
    info.clipped        = VK_TRUE;

    VK_CHECK(vkCreateSwapchainKHR(app.device, &info, NULL, &app.swapchain));

    app.swapchain_format = surface_format.format;
    app.swapchain_extent = surface_extent;

    // Get swapchain images
    uint32_t               num_images = 0;
    std::vector< VkImage > swapchain_images;
    swapchain_images.resize(app.max_frames_in_flight);

    VK_CHECK(vkGetSwapchainImagesKHR(app.device, app.swapchain, &num_images, NULL));
    ASSERT_MSG(num_images > 0, "No images available for swapchain");
    ASSERT_MSG(num_images == app.max_frames_in_flight, "Cannot get %u swapchain images",
               app.max_frames_in_flight);
    VK_CHECK(
        vkGetSwapchainImagesKHR(app.device, app.swapchain, &num_images, swapchain_images.data()));
    ASSERT_MSG(num_images > 0, "No images available for swapchain");

    app.swapchain_images.resize(app.max_frames_in_flight);

    LOG_DEBUG("Creating swapchain image views");

    for (unsigned int i = 0; i < app.swapchain_images.size(); i++) {
        VkImageViewCreateInfo image_view_create_info = {};
        image_view_create_info.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image                 = swapchain_images[i];
        image_view_create_info.viewType              = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format                = app.swapchain_format;
        // Specify component swizzle
        {
            image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_R;
            image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_G;
            image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_B;
            image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_A;
        }
        // Subresource range (basically configuring mip levels)
        {
            image_view_create_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            image_view_create_info.subresourceRange.baseMipLevel   = 0;
            image_view_create_info.subresourceRange.levelCount     = 1;
            image_view_create_info.subresourceRange.baseArrayLayer = 0;
            image_view_create_info.subresourceRange.layerCount     = 0;
        }
        image_view_create_info.flags = 0;

        VkImageView image_view;
        VK_CHECK(vkCreateImageView(app.device, &image_view_create_info, NULL, &image_view));

        app.swapchain_images[i].image      = swapchain_images[i];
        app.swapchain_images[i].image_view = image_view;
    }
}

static VkCommandPool create_command_pool(VkDevice device, int queue_index) {
    VkCommandPoolCreateInfo command_pool_create_info = {};
    command_pool_create_info.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_create_info.queueFamilyIndex = queue_index;

    VkCommandPool pool;
    VK_CHECK(vkCreateCommandPool(device, &command_pool_create_info, NULL, &pool));
    return pool;
}

static void create_frame_resources(App& app) {
    VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
    command_buffer_allocate_info.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.level       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandPool = app.command_pool;
    command_buffer_allocate_info.commandBufferCount = app.max_frames_in_flight;

    std::vector< VkCommandBuffer > command_buffers;
    command_buffers.resize(app.max_rendering_frames);
    VK_CHECK(vkAllocateCommandBuffers(app.device, &command_buffer_allocate_info,
                                      command_buffers.data()));

    app.frame_resources.resize(app.max_rendering_frames);

    VkSemaphoreCreateInfo semaphore_create_info = {};
    semaphore_create_info.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags             = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < command_buffers.size(); i++) {
        FrameResources&  frame_resources   = app.frame_resources[i];
        VkCommandBuffer& vk_command_buffer = command_buffers[i];

        frame_resources.command_buffer = vk_command_buffer;

        // Create command buffer fence
        VK_CHECK(vkCreateFence(app.device, &fence_create_info, NULL,
                               &frame_resources.draw_complete_fence));
        // Create semaphores
        VK_CHECK(vkCreateSemaphore(app.device, &semaphore_create_info, NULL,
                                   &frame_resources.acquire_semaphore));
        VK_CHECK(vkCreateSemaphore(app.device, &semaphore_create_info, NULL,
                                   &frame_resources.draw_complete_semaphore));
    }
}

bool App::image_format_supported(VkFormat format, VkFormatFeatureFlags required,
                                 VkImageTiling tiling) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(available_gpus[gpu_index].vk_physical_device, format,
                                        &props);
    VkFormatFeatureFlags flags = tiling == VK_IMAGE_TILING_OPTIMAL ? props.optimalTilingFeatures
                                                                   : props.linearTilingFeatures;
    return (flags & required) == required;
}

VkFormat App::default_depth_stencil_format() {
    if (image_format_supported(VK_FORMAT_D24_UNORM_S8_UINT,
                               VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
                               VK_IMAGE_TILING_OPTIMAL)) {
        return VK_FORMAT_D24_UNORM_S8_UINT;
    }
    if (image_format_supported(VK_FORMAT_D32_SFLOAT_S8_UINT,
                               VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
                               VK_IMAGE_TILING_OPTIMAL)) {
        return VK_FORMAT_D32_SFLOAT_S8_UINT;
    }

    return VK_FORMAT_UNDEFINED;
}

// Initialize global app

static App g_vulkan_app = {};

App& App::initialize(int width, int height, const char* name,
                     const DeviceConfig& in_device_config) {
    LOG_DEBUG("Initializing vulkan app");
    static bool vulkan_initialized = false;
    ASSERT_MSG(!vulkan_initialized, "Vulkan already initialized");

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    if (!glfwVulkanSupported()) {
        RUNTIME_ERROR("Vulkan not supported!");
    }

    DeviceConfig device_config = in_device_config;
    App&         app           = g_vulkan_app;

    app.max_frames_in_flight = device_config.max_frames_in_flight == 0
                                   ? VULKAN_DEFAULT_FRAMES_IN_FLIGHT
                                   : device_config.max_frames_in_flight;

    app.max_rendering_frames = device_config.max_rendering_frames == 0
                                   ? app.max_frames_in_flight
                                   : device_config.max_rendering_frames;

    app.window             = glfwCreateWindow(width, height, name, NULL, NULL);
    app.vk_instance        = create_instance(name, device_config);
    app.vk_debug_messenger = create_debug_messenger(app.vk_instance, NULL);
    app.vk_surface         = create_surface(app.vk_instance, app.window);
    app.gpu_index
        = pick_physical_device(app.vk_instance, device_config, app.vk_surface, app.available_gpus);
    PhysicalDevice& gpu = app.available_gpus[app.gpu_index];
    app.device          = create_logical_device(gpu, device_config);

    vkGetDeviceQueue(app.device, gpu.graphics_family_index, 0, &app.graphics_queue);
    vkGetDeviceQueue(app.device, gpu.present_family_index, 0, &app.present_queue);

    app.command_pool = create_command_pool(app.device, gpu.graphics_family_index);
    create_frame_resources(app);

    create_swapchain(app);

    vulkan_initialized = true;

    LOG_DEBUG("Vulkan app initialized");
    return app;
}

void App::deinit() {
    vkQueueWaitIdle(g_vulkan_app.graphics_queue);
    vkQueueWaitIdle(g_vulkan_app.present_queue);
    for (size_t i = 0; i < g_vulkan_app.frame_resources.size(); i++) {
        vkDestroyFence(g_vulkan_app.device, g_vulkan_app.frame_resources[i].draw_complete_fence,
                       NULL);
        vkDestroySemaphore(g_vulkan_app.device, g_vulkan_app.frame_resources[i].acquire_semaphore,
                           NULL);
        vkDestroySemaphore(g_vulkan_app.device,
                           g_vulkan_app.frame_resources[i].draw_complete_semaphore, NULL);
    }

    // TODO Delete render passes
    LOG_WARNING("CURRENTLY LEAKING RENDER PASSES");
    // TODO Delete framebuffers
    LOG_WARNING("CURRENTLY LEAKING FRAMEBUFFERS");

    vkDestroyCommandPool(g_vulkan_app.device, g_vulkan_app.command_pool, NULL);
    for (size_t i = 0; i < g_vulkan_app.swapchain_images.size(); i++) {
        vkDestroyImageView(g_vulkan_app.device, g_vulkan_app.swapchain_images[i].image_view, NULL);
    }
    vkDestroySwapchainKHR(g_vulkan_app.device, g_vulkan_app.swapchain, NULL);
    vkDestroyDevice(g_vulkan_app.device, NULL);
#ifdef APP_DEBUG
    vkDestroyDebugUtilsMessengerEXT(g_vulkan_app.vk_instance, g_vulkan_app.vk_debug_messenger,
                                    NULL);
#endif

    vkDestroySurfaceKHR(g_vulkan_app.vk_instance, g_vulkan_app.vk_surface, NULL);
    vkDestroyInstance(g_vulkan_app.vk_instance, NULL);

    glfwDestroyWindow(g_vulkan_app.window);
    glfwTerminate();
}

}    // namespace Vulkan