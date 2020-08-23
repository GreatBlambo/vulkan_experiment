#pragma once

#include "../vulkan_app.h"
#include "../vulkan_resource_manager.h"
#include "../memory.h"

class Demo {
  public:
    virtual void init(Vulkan::App& app, Vulkan::ResourceManager& resource_manager,
                      Memory::VirtualHeap& demo_heap)
        = 0;
    virtual void render(Vulkan::App& app, Vulkan::ResourceManager& resource_manager,
                        Memory::VirtualHeap& frame_heap)
        = 0;
    virtual void destroy(Vulkan::App& app) = 0;

  protected:
    void render_frame(Vulkan::App& app,
                      const std::function< void(const size_t, VkCommandBuffer) >& render);
};