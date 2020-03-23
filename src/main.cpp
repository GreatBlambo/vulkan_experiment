#include "vulkan_app.h"
#include "vulkan_utils.h"
#include "handle.h"
#include "utils.h"

#include <vector>
#include <algorithm>
#include <queue>
#include <array>
#include <unordered_set>

/////////////////////////////////////////////////////////////////////////////////////////////////
// Render Passes //////////////////////////////////////////////////////////////////////////////// 
/////////////////////////////////////////////////////////////////////////////////////////////////

struct RenderPasses {
    std::vector< VkFramebuffer > swapchain_framebuffers;
    VkRenderPass                 final_pass;
};

static void create_render_passes(Vulkan::App& app, RenderPasses& render_passes) {
    // Create final render pass - this outputs to the swapchain
    {
        // Output color attachment for swapchain
        VkAttachmentDescription swapchain_color_attachment = {};
        swapchain_color_attachment.format                  = app.swapchain_format;
        swapchain_color_attachment.samples                 = VK_SAMPLE_COUNT_1_BIT;
        swapchain_color_attachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        swapchain_color_attachment.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
        swapchain_color_attachment.stencilLoadOp           = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        swapchain_color_attachment.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        swapchain_color_attachment.finalLayout             = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // Define subpass
        // This is attached to the subpass to specify which attachment indexes to reference
        VkAttachmentReference swapchain_color_attachment_reference = {};
        swapchain_color_attachment_reference.attachment            = 0;
        swapchain_color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &swapchain_color_attachment_reference;

        // Define final render pass
        VkRenderPassCreateInfo final_pass_create_info = {};
        final_pass_create_info.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        final_pass_create_info.attachmentCount        = 1;
        final_pass_create_info.pAttachments           = &swapchain_color_attachment;
        final_pass_create_info.subpassCount           = 1;
        final_pass_create_info.pSubpasses             = &subpass;

        // Create the final render pass
        VK_CHECK(vkCreateRenderPass(app.device, &final_pass_create_info, nullptr,
                                    &render_passes.final_pass));
    }

    // Create framebuffers
    render_passes.swapchain_framebuffers.reserve(app.swapchain_images.size());
    for (auto& swapchain_image : app.swapchain_images) {
        VkImageView& swapchain_image_view = swapchain_image.image_view;

        VkImageView attachments[] = { swapchain_image_view };

        VkFramebufferCreateInfo swapchain_framebuffer_create_info = {};
        swapchain_framebuffer_create_info.sType      = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        swapchain_framebuffer_create_info.renderPass = render_passes.final_pass;
        swapchain_framebuffer_create_info.attachmentCount = ARRAY_LENGTH(attachments);
        swapchain_framebuffer_create_info.pAttachments    = attachments;
        swapchain_framebuffer_create_info.width           = app.swapchain_extent.width;
        swapchain_framebuffer_create_info.height          = app.swapchain_extent.height;
        swapchain_framebuffer_create_info.layers          = 1;

        VkFramebuffer new_framebuffer;
        VK_CHECK(vkCreateFramebuffer(app.device, &swapchain_framebuffer_create_info, nullptr,
                                     &new_framebuffer));

        render_passes.swapchain_framebuffers.push_back(new_framebuffer);
    }
}

static void destroy_render_passes(Vulkan::App& app, RenderPasses& render_passes) {
    vkDestroyRenderPass(app.device, render_passes.final_pass, nullptr);
    for (auto& framebuffer : render_passes.swapchain_framebuffers) {
        vkDestroyFramebuffer(app.device, framebuffer, nullptr);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// Material Resources /////////////////////////////////////////////////////////////////////////// 
/////////////////////////////////////////////////////////////////////////////////////////////////

STRONGLY_TYPED_WEAKREF(ShaderModule);

struct MaterialResources {
};

void create_material_resources(Vulkan::App& app, MaterialResources& material_resources) {
}

void destroy_material_resources(Vulkan::App& app, MaterialResources& material_resources) {
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// Execution //////////////////////////////////////////////////////////////////////////////////// 
/////////////////////////////////////////////////////////////////////////////////////////////////

static void execute_draw_commands(size_t swapchain_index, RenderPasses& render_passes) {

}

static void render_frame(Vulkan::App& app, RenderPasses& render_passes) {
    // Advance to a new frame
    size_t last_frame    = app.current_frame;
    size_t current_frame = app.current_frame = (app.current_frame + 1) % app.max_rendering_frames;

    LOG_DEBUG("Rendering frame %u", app.current_frame);

    // Get frame resources
    Vulkan::FrameResources& frame_resources = app.frame_resources[current_frame];

    // Wait for the last queue that used these frame resources to finish rendering. This is mainly
    // to signal it's safe to reuse the command buffer
    vkWaitForFences(app.device, 1, &frame_resources.draw_complete_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(app.device, 1, &frame_resources.draw_complete_fence);

    LOG_DEBUG("Acquiring swapchain image");

    // Acquire a swapchain image
    uint32_t image_index;
    vkAcquireNextImageKHR(app.device, app.swapchain, UINT64_MAX, frame_resources.acquire_semaphore,
                          VK_NULL_HANDLE, &image_index);

    // Record/supply command buffers
    {
        vkResetCommandBuffer(frame_resources.command_buffer, 0);
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(frame_resources.command_buffer, &begin_info);

        // Draw commands
        execute_draw_commands(image_index, render_passes);

        vkEndCommandBuffer(frame_resources.command_buffer);
    }

    // Submit to queue
    {
        VkSubmitInfo submit_info = {};
        submit_info.sType        = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        // Add wait semaphores. At the very least, do not output color until the image has been
        // acquired.
        std::array< VkSemaphore, 1 > wait_semaphores = { frame_resources.acquire_semaphore };
        std::array< VkPipelineStageFlags, 1 > wait_stages
            = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submit_info.waitSemaphoreCount = wait_semaphores.size();
        submit_info.pWaitSemaphores    = wait_semaphores.data();
        submit_info.pWaitDstStageMask  = wait_stages.data();

        // Add signal semaphores
        std::array< VkSemaphore, 1 > signal_semaphores
            = { frame_resources.draw_complete_semaphore };
        submit_info.signalSemaphoreCount = signal_semaphores.size();
        submit_info.pSignalSemaphores    = signal_semaphores.data();

        // Add command buffers
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers    = &frame_resources.command_buffer;

        VK_CHECK(vkQueueSubmit(app.graphics_queue, 1, &submit_info,
                               frame_resources.draw_complete_fence));
    }

    // Present swapchain image
    {
        VkPresentInfoKHR present_info   = {};
        present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores    = &frame_resources.draw_complete_semaphore;
        present_info.swapchainCount     = 1;
        present_info.pSwapchains        = &app.swapchain;
        present_info.pImageIndices      = &image_index;

        VK_CHECK(vkQueuePresentKHR(app.present_queue, &present_info));
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// App main ///////////////////////////////////////////////////////////////////////////////////// 
/////////////////////////////////////////////////////////////////////////////////////////////////

int main() {
    Vulkan::DeviceConfig device_config;
    device_config.validation_layers = std::vector< const char* >({ "VK_LAYER_KHRONOS_validation" });
    device_config.instance_extensions = std::vector< const char* >({ "VK_EXT_debug_utils" });
    device_config.device_extensions
        = std::vector< const char* >({ VK_KHR_SWAPCHAIN_EXTENSION_NAME });

    Vulkan::App& app = Vulkan::App::initialize(800, 600, "App", device_config);

    RenderPasses render_passes;
    create_render_passes(app, render_passes);

    while (!glfwWindowShouldClose(app.window)) {
        glfwPollEvents();
        render_frame(app, render_passes);
    }

    destroy_render_passes(app, render_passes);
    Vulkan::App::deinit();

    return 0;
}