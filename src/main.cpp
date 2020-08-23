#include <vector>
#include <algorithm>

#include <glm/glm.hpp>

#include "vulkan_app.h"
#include "vulkan_utils.h"
#include "utils.h"
#include "file_system.h"
#include "vulkan_resource_manager.h"
#include "memory.h"

#include "demos/demo.h"
#include "demos/triangle.h"

/////////////////////////////////////////////////////////////////////////////////////////////////
// App main /////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////


static Triangle demo_triangle;
static Demo* demos[] = {
    &demo_triangle
};
static Demo* current_demo = nullptr;

void start_demo(int demo_index,
                Vulkan::App& app,
                Vulkan::ResourceManager& resource_manager,
                Memory::VirtualHeap& demo_heap) {
    if (demo_index < 0 || demo_index >= ARRAYSIZE(demos)) {
        return;
    }
    if (current_demo == demos[demo_index]) {
        return;
    }

    // Destroy current demo
    if (current_demo) {
        current_demo->destroy(app);
    }
    resource_manager.clear();
    demo_heap.clear();

    current_demo = demos[demo_index];
    current_demo->init(app, resource_manager, demo_heap);
}

int main() {
    Memory::VirtualHeap app_heap(GB(8));
    Memory::VirtualHeap demo_heap(GB(8));
    Memory::VirtualHeap per_frame_heap(GB(8));

    // Load external resources
    FileSystem::initialize("./");

    // Configure vulkan app
    Vulkan::DeviceConfig device_config;
    device_config.validation_layers = std::vector< const char* >({ "VK_LAYER_KHRONOS_validation" });
    device_config.instance_extensions = std::vector< const char* >({ "VK_EXT_debug_utils" });
    device_config.device_extensions
        = std::vector< const char* >({ VK_KHR_SWAPCHAIN_EXTENSION_NAME });

    Vulkan::App app(800, 600, "App", device_config);

    Vulkan::ResourceManager resource_manager(app, app_heap);

    start_demo(0, app, resource_manager, demo_heap);

    // Render loop
    while (!glfwWindowShouldClose(app.window)) {
        glfwPollEvents();
        per_frame_heap.clear();
        current_demo->render(app, resource_manager, per_frame_heap);
    }

    // Clean up resources
    current_demo->destroy(app);

    // Deinit fs
    FileSystem::deinit();

    return 0;
}