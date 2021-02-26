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
#include "demos/vertex_buffers.h"

/////////////////////////////////////////////////////////////////////////////////////////////////
// App main /////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////


static TriangleDemo triangle_demo;
static VertexBuffersDemo vertex_buffers_demo;
static Demo* demos[] = {
    &triangle_demo,
    &vertex_buffers_demo
};
static int current_demo_index = -1;

void end_demo(int demo_index, Vulkan::App& app);

void start_demo(int demo_index,
                Vulkan::App& app,
                Vulkan::ResourceManager& resource_manager,
                Memory::VirtualHeap& demo_heap) {
    if (demo_index < 0 || demo_index >= ARRAYSIZE(demos)) {
        return;
    }
    if (current_demo_index == demo_index) {
        return;
    }

    // Destroy current demo
    end_demo(demo_index, app);
    resource_manager.clear();
    demo_heap.clear();

    current_demo_index = demo_index;
    demos[current_demo_index]->init(app, resource_manager, demo_heap);
}

void step_demo(int demo_index, 
               Vulkan::App& app, 
               Vulkan::ResourceManager& resource_manager,
               Memory::VirtualHeap& frame_heap) {
    demos[demo_index]->render(app, resource_manager, frame_heap);
}

void end_demo(int demo_index, Vulkan::App& app) {
    if (demo_index < 0) {
        return;
    }
    app.wait_for_device();
    demos[demo_index]->destroy(app);
}

int main() {
    Memory::VirtualHeap app_heap(GB(8));
    Memory::VirtualHeap demo_heap(GB(8));
    Memory::VirtualHeap frame_heap(GB(8));

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
        frame_heap.clear();
        step_demo(current_demo_index, app, resource_manager, frame_heap);
    }

    // Clean up resources
    end_demo(current_demo_index, app);

    // Deinit fs
    FileSystem::deinit();

    return 0;
}