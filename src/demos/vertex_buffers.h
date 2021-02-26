#pragma once

#include <vector>
#include <memory.h>

#include "demo.h"

class VertexBuffersDemo : public Demo {
  public:
    void init(Vulkan::App& app, Vulkan::ResourceManager& resource_manager,
              Memory::VirtualHeap& demo_heap);
    void render(Vulkan::App& app, Vulkan::ResourceManager& resource_manager,
                Memory::VirtualHeap& frame_heap);
    void destroy(Vulkan::App& app);
  private:
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    std::vector< VkFramebuffer > swapchain_framebuffers;
    VkRenderPass final_pass;
};