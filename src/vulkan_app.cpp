#include "vulkan_app.h"
#include "vulkan_utils.h"
#include "utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <algorithm>

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
                                                     VkDebugUtilsMessageTypeFlagsEXT             message_type,
                                                     const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
                                                     void*                                       p_user_data)
{
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

static VkResult vkCreateDebugUtilsMessengerEXT(VkInstance                                instance,
                                               const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                               const VkAllocationCallbacks*              pAllocator,
                                               VkDebugUtilsMessengerEXT*                 pCallback)
{
    static PFN_vkCreateDebugUtilsMessengerEXT func
        = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pCallback);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static void vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback,
                                            const VkAllocationCallbacks* pAllocator)
{
    static PFN_vkDestroyDebugUtilsMessengerEXT func
        = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, callback, pAllocator);
    }
}

static void enable_validation_layers(VkInstanceCreateInfo& create_info, const VulkanExtensionInfo& extension_info)
{
    // Check for validation layers
    if (extension_info.validation_layers.size() <= 0)
    {
        create_info.enabledLayerCount = 0;
        return;
    }

    uint32_t vl_count = 0;
    vkEnumerateInstanceLayerProperties(&vl_count, NULL);

    VkLayerProperties* supported_layers = (VkLayerProperties*) malloc(sizeof(VkLayerProperties) * vl_count);
    vkEnumerateInstanceLayerProperties(&vl_count, supported_layers);

    for (size_t i = 0; i < extension_info.validation_layers.size(); i++)
    {
        bool supported = false;
        for (size_t j = 0; j < vl_count; j++)
        {
            if (strcmp(supported_layers[j].layerName, extension_info.validation_layers[i]) == 0)
            {
                supported = true;
                break;
            }
        }

        if (!supported)
        {
            fprintf(stderr, "Error: validation layer %s is not supported\n", extension_info.validation_layers[i]);
            exit(1);
        }
    }

    create_info.enabledLayerCount   = extension_info.validation_layers.size();
    create_info.ppEnabledLayerNames = extension_info.validation_layers.data();
    free(supported_layers);
}

static void add_extensions(VkInstanceCreateInfo& create_info, VulkanExtensionInfo& extension_info)
{
    // Get extensions
    uint32_t     req_extension_count = 0;
    const char** req_extensions      = glfwGetRequiredInstanceExtensions(&req_extension_count);

    for (uint32_t i = 0; i < req_extension_count; i++)
    {
        extension_info.instance_extensions.push_back(req_extensions[i]);
    }

    create_info.enabledExtensionCount   = extension_info.instance_extensions.size();
    create_info.ppEnabledExtensionNames = extension_info.instance_extensions.data();
}

static VkInstance create_instance(const char* name, VulkanExtensionInfo& extension_info)
{
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
    enable_validation_layers(create_info, extension_info);

    // Add extensions
    add_extensions(create_info, extension_info);

    // Create instance
    VkInstance instance;
    if (vkCreateInstance(&create_info, NULL, &instance) != VK_SUCCESS)
    {
        RUNTIME_ERROR("Failed to create instance");
    }

    return instance;
}

static VkDebugUtilsMessengerEXT create_debug_messenger(VkInstance instance, const VkAllocationCallbacks* allocator)
{

#ifdef APP_DEBUG
    // Create debug callback object
    VkDebugUtilsMessengerCreateInfoEXT error_cb_create_info = {};
    error_cb_create_info.sType                              = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    error_cb_create_info.messageSeverity                    = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                                           | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                           | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    error_cb_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                                       | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                                       | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    error_cb_create_info.pfnUserCallback = debug_callback;
    error_cb_create_info.pUserData       = NULL;

    VkDebugUtilsMessengerEXT messenger;
    VkResult status = vkCreateDebugUtilsMessengerEXT(instance, &error_cb_create_info, allocator, &messenger);
    assert(status != VK_ERROR_EXTENSION_NOT_PRESENT);

    return messenger;
#else
    return VK_NULL_HANDLE;
#endif
}

static const VulkanPhysicalDevice* pick_physical_device(VkInstance instance, const VulkanExtensionInfo& extension_info,
                                                        VkSurfaceKHR surface, std::vector< VulkanPhysicalDevice >& out_gpus)
{
    // Populate physical device information
    uint32_t device_count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &device_count, NULL));

    ASSERT(device_count > 0);

    std::vector< VkPhysicalDevice > devices(device_count);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &device_count, devices.data()));

    out_gpus.resize(device_count);
    for (size_t i = 0; i < device_count; i++)
    {
        VulkanPhysicalDevice& gpu = out_gpus[i];

        gpu.vk_physical_device = devices[i];
        // Get physical device properties
        vkGetPhysicalDeviceProperties(gpu.vk_physical_device, &gpu.vk_physical_device_props);

        // Get physical device vk_physical_device_features
        vkGetPhysicalDeviceFeatures(gpu.vk_physical_device, &gpu.vk_physical_device_features);

        // Get physical device memory properties
        vkGetPhysicalDeviceMemoryProperties(gpu.vk_physical_device, &gpu.vk_physical_device_mem_props);

        // Get supported device extensions
        {
            uint32_t num_extensions;
            VK_CHECK(vkEnumerateDeviceExtensionProperties(gpu.vk_physical_device, NULL, &num_extensions, NULL));
			ASSERT_MSG(num_extensions > 0, "vkEnumerateDeviceExtensionProperties returned zero extensions.");

            gpu.vk_extension_props.resize(num_extensions);
            VK_CHECK(vkEnumerateDeviceExtensionProperties(gpu.vk_physical_device, NULL, &num_extensions, gpu.vk_extension_props.data()));
        }

        // Get device queues
        {
            uint32_t num_queues;
            vkGetPhysicalDeviceQueueFamilyProperties(gpu.vk_physical_device, &num_queues, NULL);
			ASSERT_MSG(num_queues > 0, "vkGetPhysicalDeviceQueueFamilyProperties returned zero queue properties.");

            gpu.vk_queue_props.resize(num_queues);
            vkGetPhysicalDeviceQueueFamilyProperties(gpu.vk_physical_device, &num_queues, gpu.vk_queue_props.data());
        }

        // Get surface capabilities
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu.vk_physical_device, surface, &gpu.vk_surface_capabilities));

        // Get supported surface formats
        {
            uint32_t num_formats;
			VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.vk_physical_device, surface, &num_formats, NULL));
			ASSERT_MSG(num_formats > 0, "vkGetPhysicalDeviceSurfaceFormatsKHR returned zero surface formats.");

            gpu.vk_surface_formats.resize(num_formats);
            VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.vk_physical_device, surface, &num_formats, gpu.vk_surface_formats.data()));
        }

        // Get supported presentation modes
        {
            uint32_t num_presentation_modes;
            VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.vk_physical_device, surface, &num_presentation_modes, NULL));
            ASSERT_MSG(num_presentation_modes > 0, "vkGetPhysicalDeviceSurfacePresentModesKHR returned zero presentation modes.");

            gpu.vk_presentation_modes.resize(num_presentation_modes);
            VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.vk_physical_device, surface, &num_presentation_modes, gpu.vk_presentation_modes.data()));
        }

        LOG_DEBUG("Physical device detected: %s", gpu.vk_physical_device_props.deviceName);
    }

    // Pick physical device
    uint32_t                    max_score   = 0;
    const VulkanPhysicalDevice* best_device = NULL;
    for (const VulkanPhysicalDevice& gpu : out_gpus)
    {
        // Check for required vk_physical_device_features
        {
            // Check extensions supported by device
            std::string unsupported_extensions = "";
            for (const char* extension : extension_info.device_extensions)
            {
                auto compare = [&extension](const VkExtensionProperties& extension_prop) {
                    return strcmp(extension_prop.extensionName, extension) == 0;
                };

                if (std::find_if(gpu.vk_extension_props.begin(), gpu.vk_extension_props.end(), compare)
                    == gpu.vk_extension_props.end())
                {
                    unsupported_extensions += extension;
                    unsupported_extensions += ", ";
                }
            }

            if (unsupported_extensions.length() > 0)
            {
                LOG_DEBUG("Skipping %s since it does not support extensions %s", gpu.vk_physical_device_props.deviceName,
                          unsupported_extensions.data());
                continue;
            }
        }

        // Score device
        uint32_t score = 0;

        struct
        {
            uint32_t discrete_gpu     = 1 << 10;
            uint32_t geometry_shaders = 1;
        } support_priorities;

        if (gpu.vk_physical_device_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            score += support_priorities.discrete_gpu;
        }

        if (gpu.vk_physical_device_features.geometryShader)
        {
            score += support_priorities.geometry_shaders;
        }

        // Select device if it scores the best so far
        if (score >= max_score)
        {
            max_score   = score;
            best_device = &gpu;
        }
    }

    return best_device;
}

static VkSurfaceKHR create_surface(VkInstance instance, GLFWwindow *window) 
{
    VkSurfaceKHR surface;
    VK_CHECK(glfwCreateWindowSurface(instance, window, NULL, &surface));
    return surface;
}

VulkanApp vulkan_init(int width, int height, const char* name, const VulkanExtensionInfo* p_extension_info)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    if (!glfwVulkanSupported())
    {
        RUNTIME_ERROR("Vulkan not supported!");
    }

    VulkanExtensionInfo extension_info;
    if (p_extension_info)
    {
        extension_info = *p_extension_info;
    }

    VulkanApp new_app          = {};
    new_app.window             = glfwCreateWindow(width, height, name, NULL, NULL);
    new_app.vk_instance        = create_instance(name, extension_info);
    new_app.vk_debug_messenger = create_debug_messenger(new_app.vk_instance, NULL);
    new_app.vk_surface         = create_surface(new_app.vk_instance, new_app.window);
    new_app.gpu                = pick_physical_device(new_app.vk_instance, extension_info, new_app.vk_surface, new_app.available_gpus);

    return new_app;
}

void vulkan_deinit(VulkanApp* app)
{
#ifdef APP_DEBUG
    vkDestroyDebugUtilsMessengerEXT(app->vk_instance, app->vk_debug_messenger, NULL);
#endif

    vkDestroySurfaceKHR(app->vk_instance, app->vk_surface, NULL);
    vkDestroyInstance(app->vk_instance, NULL);

    glfwDestroyWindow(app->window);
    glfwTerminate();
}
