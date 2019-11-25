#include "vulkan_app.h"
#include "utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
                                                     VkDebugUtilsMessageTypeFlagsEXT             message_type,
                                                     const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
                                                     void*                                       p_user_data)
{
    char* prefix = "Validation layer: %s";
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

static void enable_validation_layers(VkInstanceCreateInfo& create_info, const VulkanExtensionInfo* extension_info)
{
    // Check for validation layers
    if (!extension_info || extension_info->num_validation_layers <= 0)
    {
        create_info.enabledLayerCount = 0;
        return;
    }

    uint32_t vl_count = 0;
    vkEnumerateInstanceLayerProperties(&vl_count, NULL);

    VkLayerProperties* supported_layers = (VkLayerProperties*) malloc(sizeof(VkLayerProperties) * vl_count);
    vkEnumerateInstanceLayerProperties(&vl_count, supported_layers);

    for (size_t i = 0; i < extension_info->num_validation_layers; i++)
    {
        bool supported = false;
        for (size_t j = 0; j < vl_count; j++)
        {
            if (strcmp(supported_layers[j].layerName, extension_info->validation_layers[i]) == 0)
            {
                supported = true;
                break;
            }
        }

        if (!supported)
        {
            fprintf(stderr, "Error: validation layer %s is not supported\n", extension_info->validation_layers[i]);
            exit(1);
        }
    }

    create_info.enabledLayerCount   = extension_info->num_validation_layers;
    create_info.ppEnabledLayerNames = extension_info->validation_layers;
    free(supported_layers);
}

static VkInstance create_instance(const char* name, const VulkanExtensionInfo* extension_info)
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

    // TODO: Create debug callback

    create_info.enabledLayerCount = 0;
    enable_validation_layers(create_info, extension_info);

    // Get extensions
    bool         has_extensions      = extension_info && extension_info->extensions;
    uint32_t     req_extension_count = 0;
    const char** req_extensions      = glfwGetRequiredInstanceExtensions(&req_extension_count);

    const uint32_t enabled_extension_count = has_extensions ? extension_info->num_extensions : 0;
    const uint32_t extension_count         = req_extension_count + enabled_extension_count;

    const char** extensions = (const char**) malloc(sizeof(const char*) * extension_count);
    memcpy(extensions, req_extensions, sizeof(const char*) * req_extension_count);
    if (has_extensions)
    {
        memcpy(&extensions[req_extension_count], extension_info->extensions,
               sizeof(const char*) * enabled_extension_count);
    }

    create_info.enabledExtensionCount   = extension_count;
    create_info.ppEnabledExtensionNames = extensions;

    // Create instance
    VkInstance instance;
    if (vkCreateInstance(&create_info, NULL, &instance) != VK_SUCCESS)
    {
        RUNTIME_ERROR("Failed dto create instance");
    }

    free(extensions);

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

static VkPhysicalDevice pick_physical_device(VkInstance instance)
{
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);

    assert(device_count > 0);

    VkPhysicalDevice* devices = (VkPhysicalDevice*) malloc(sizeof(VkPhysicalDevice) * device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices);

    uint32_t         max_score   = 0;
    VkPhysicalDevice best_device = VK_NULL_HANDLE;
    for (size_t i = 0; i < device_count; i++)
    {
        uint32_t score = 0;

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(devices[i], &properties);

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(devices[i], &features);

        LOG_DEBUG("Device detected: %s", properties.deviceName);

        struct
        {
            uint32_t discrete_gpu     = 1 << 10;
            uint32_t geometry_shaders = 1;
        } support_priorities;

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            score += support_priorities.discrete_gpu;
        }

        if (features.geometryShader)
        {
            score += support_priorities.geometry_shaders;
        }

        if (score >= max_score)
        {
            max_score   = score;
            best_device = devices[i];
        }
    }

    free(devices);

    return best_device;
}

VulkanApp vulkan_init(int width, int height, const char* name, const VulkanExtensionInfo* extension_info)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    if (!glfwVulkanSupported())
    {
        RUNTIME_ERROR("Vulkan not supported!");
    }

    VulkanApp new_app          = {};
    new_app.window             = glfwCreateWindow(width, height, name, NULL, NULL);
    new_app.vk_instance        = create_instance(name, extension_info);
    new_app.vk_debug_messenger = create_debug_messenger(new_app.vk_instance, NULL);
    new_app.vk_physical_device = pick_physical_device(new_app.vk_instance);

    return new_app;
}

void vulkan_deinit(VulkanApp* app)
{
#ifdef APP_DEBUG
    vkDestroyDebugUtilsMessengerEXT(app->vk_instance, app->vk_debug_messenger, NULL);
#endif

    vkDestroyInstance(app->vk_instance, NULL);

    glfwDestroyWindow(app->window);
    glfwTerminate();
}
