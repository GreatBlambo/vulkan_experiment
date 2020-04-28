#include "rendering.h"
#include "vulkan_utils.h"

#include <array>

/////////////////////////////////////////////////////////////////////////////////////////////////
// Execution ////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////

namespace Vulkan {

void render_frame(Vulkan::App& app, std::function< void(const size_t, VkCommandBuffer) > render) {
    // Advance to a new frame
    size_t last_frame    = app.current_frame;
    size_t current_frame = app.current_frame = (app.current_frame + 1) % app.max_rendering_frames;

    // Get frame resources
    Vulkan::FrameResources& frame_resources = app.frame_resources[current_frame];

    // Wait for the last queue that used these frame resources to finish rendering. This is mainly
    // to signal it's safe to reuse the command buffer
    vkWaitForFences(app.device, 1, &frame_resources.draw_complete_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(app.device, 1, &frame_resources.draw_complete_fence);

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
        render(image_index, frame_resources.command_buffer);

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
}