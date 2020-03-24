#include "vulkan_app.h"
#include "vulkan_utils.h"
#include "handle.h"
#include "utils.h"

#include <vector>
#include <algorithm>
#include <queue>
#include <array>
#include <unordered_set>

#include <functional>

#include <physfs.h>

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

// Material resources include any vulkan objects required to make a material.
// The intention here is to allow the user to string together configurations on the fly and we will reuse
// vulkan objects as necessary.

struct MaterialTable {
};

void create_material_table(Vulkan::App& app, MaterialTable& material_table) {
}

void destroy_material_table(Vulkan::App& app, MaterialTable& material_table) {
}

struct ShaderModuleInfo {
    const char* name;
    uint32_t* spirv_data;
    size_t size;
};

// We can do these find_* functions even with create info structs which contain non-dispatchable handles because
// in all likelihood if two handles have the same value, the implementation believes them to be equivalent. So
// they aren't really collisions, in this case.

VkPipelineLayout find_pipeline_layout(MaterialTable& material_table, const VkPipelineLayoutCreateInfo& create_info) {
    return VK_NULL_HANDLE;
}

VkPipeline find_pipeline(MaterialTable& material_table, const VkGraphicsPipelineCreateInfo& create_info) {
    return VK_NULL_HANDLE;
}

VkDescriptorSetLayout find_descriptor_set_layout(MaterialTable& material_table, const VkDescriptorSetLayoutCreateInfo& create_info) {
    return VK_NULL_HANDLE;
}

VkPipeline create_pipeline(MaterialTable& material_table, ShaderModuleInfo* modules, size_t num_modules) {
    for (size_t i = 0; i < num_modules; i++) {
        const ShaderModuleInfo& module = modules[i];
        // Check if any descriptor layouts can be reused
    }

    // Check if any pipeline layouts can be reused
    //      If so, check if any pipelines can be reused.
    //      If not, create pipeline layout and pipeline

    return VK_NULL_HANDLE;
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
// Files //////////////////////////////////////////////////////////////////////////////////////// 
/////////////////////////////////////////////////////////////////////////////////////////////////

struct FileResult {
    const char* filename;
    uint8_t* data;
    size_t size;
};

void load_temp_file(const char* filename, std::function<void(const FileResult&)> on_file_load) {
    int result = PHYSFS_exists(filename);
    if (!result) {
        RUNTIME_ERROR("Error loading %s", filename);
    }

    PHYSFS_file* file = PHYSFS_openRead(filename);
    PHYSFS_sint64 file_size = PHYSFS_fileLength(file);

    uint8_t* buffer = new uint8_t[file_size];
    PHYSFS_read(file, buffer, 1, file_size);
    PHYSFS_close(file); 

    on_file_load({
        filename,
        buffer, 
        (size_t) file_size
    });
    delete[] buffer;
}

void load_temp_files(const char** filenames, size_t num_files, std::function<void(const FileResult*, size_t)> on_files_load) {
    std::vector<FileResult> results;
    for (size_t i = 0; i < num_files; i++) {
        const char* filename = filenames[i];
        int result = PHYSFS_exists(filename);
        if (!result) {
            RUNTIME_ERROR("Error loading %s", filename);
        }

        PHYSFS_file* file = PHYSFS_openRead(filename);
        PHYSFS_sint64 file_size = PHYSFS_fileLength(file);

        uint8_t* buffer = new uint8_t[file_size];
        PHYSFS_read(file, buffer, 1, file_size);
        PHYSFS_close(file); 

        results.push_back({
            filename,
            buffer,
            (size_t) file_size
        });
    }

    on_files_load(results.data(), results.size());

    for (const FileResult& result : results) {
        delete[] result.data;
    }
}

void init_fs() {
    PHYSFS_init(NULL);
    if (!PHYSFS_mount("./shaders", "shaders", 1)) {
        RUNTIME_ERROR("Failed to mount shaders folder");
    }
}

void deinit_fs() {
    PHYSFS_deinit();
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// App main ///////////////////////////////////////////////////////////////////////////////////// 
/////////////////////////////////////////////////////////////////////////////////////////////////

int main() {
    // TODO: Memory arenas
    // Load external resources
    init_fs();

    // Configure vulkan app
    Vulkan::DeviceConfig device_config;
    device_config.validation_layers = std::vector< const char* >({ "VK_LAYER_KHRONOS_validation" });
    device_config.instance_extensions = std::vector< const char* >({ "VK_EXT_debug_utils" });
    device_config.device_extensions
        = std::vector< const char* >({ VK_KHR_SWAPCHAIN_EXTENSION_NAME });

    Vulkan::App& app = Vulkan::App::initialize(800, 600, "App", device_config);

    // Create vulkan resources
    MaterialTable material_table;
    create_material_table(app, material_table);

    // TEMP CODE
    // This is what it would look like to create one pipeline from shader stages (and also additional settings)
    // Can probably load this from a json eventually, make it data driven
    VkPipeline pipeline;
    const char* shader_files[] = {"shaders/test.vert.spv", "shaders/test.frag.spv"};
    load_temp_files(shader_files, ARRAY_LENGTH(shader_files), [shader_files, &pipeline, &material_table](const FileResult* results, size_t num_results) {
        ShaderModuleInfo test_shader_module[2];
        test_shader_module[0].name = shader_files[0];
        test_shader_module[0].spirv_data = (uint32_t*) results[0].data;
        test_shader_module[0].size = results[0].size / sizeof(uint32_t);

        test_shader_module[1].name = shader_files[1];
        test_shader_module[1].spirv_data = (uint32_t*) results[1].data;
        test_shader_module[1].size = results[1].size / sizeof(uint32_t);

        pipeline = create_pipeline(material_table, test_shader_module, 2);
    });

    RenderPasses render_passes;
    create_render_passes(app, render_passes);

    // Render loop
    while (!glfwWindowShouldClose(app.window)) {
        glfwPollEvents();
        render_frame(app, render_passes);
    }

    // Clean up resources
    destroy_render_passes(app, render_passes);

    destroy_material_table(app, material_table);

    // Deinit vulkan app
    Vulkan::App::deinit();

    // Deinit fs
    deinit_fs();

    return 0;
}