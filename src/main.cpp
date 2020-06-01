#include "vulkan_app.h"
#include "vulkan_utils.h"
#include "handle.h"
#include "utils.h"
#include "file_system.h"
#include "renderer.h"
#include "memory.h"

#include <vector>
#include <algorithm>

#include <glm/glm.hpp>

/////////////////////////////////////////////////////////////////////////////////////////////////
// App main /////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////

int main() {
    Memory::VirtualHeap app_heap(GB(8));
    Memory::VirtualHeap per_frame_heap(GB(8));

    // Load external resources
    FileSystem::initialize("./");

    // Configure vulkan app
    Vulkan::DeviceConfig device_config;
    device_config.validation_layers = std::vector< const char* >({ "VK_LAYER_KHRONOS_validation" });
    device_config.instance_extensions = std::vector< const char* >({ "VK_EXT_debug_utils" });
    device_config.device_extensions
        = std::vector< const char* >({ VK_KHR_SWAPCHAIN_EXTENSION_NAME });

    Vulkan::App app(1920, 1080, "App", device_config);

    // Create vulkan resources
    Vulkan::Renderer renderer(app, app_heap);

    // TEMP CODE
    VkPipelineLayout pipeline_layout;

    const char* shader_files[] = { "shaders/triangle.vert.spv", "shaders/triangle.vert.json",
                                   "shaders/triangle.frag.spv", "shaders/triangle.frag.json" };

    std::vector< Vulkan::ShaderModule > shader_modules;
    FileSystem::load_temp_files(
        shader_files, ARRAY_LENGTH(shader_files),
        [&shader_modules, &renderer](const Memory::Buffer* results, size_t num_results) {
            const Memory::Buffer& test_vert_spv_file  = results[0];
            const Memory::Buffer& test_vert_json_file = results[1];
            const Memory::Buffer& test_frag_spv_file  = results[2];
            const Memory::Buffer& test_frag_json_file = results[3];

            shader_modules.push_back(renderer.create_shader_module("test_vert", test_vert_spv_file, test_vert_json_file));
            shader_modules.push_back(renderer.create_shader_module("test_frag", test_frag_spv_file, test_frag_json_file));
        });
    pipeline_layout = renderer.create_pipeline_layout_from_modules(shader_module_data);

    // Create render passes and framebuffers
    // This is some temp code pretty much ripped from the vulkan tutorial. These
    // will be in a render graph created from render pass configs

    std::vector< VkFramebuffer > swapchain_framebuffers;
    VkRenderPass final_pass;
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
                                    &final_pass));
    }

    // Create framebuffers
    swapchain_framebuffers.reserve(app.swapchain_images.size());
    for (auto& swapchain_image : app.swapchain_images) {
        VkImageView& swapchain_image_view = swapchain_image.image_view;

        VkImageView attachments[] = { swapchain_image_view };

        VkFramebufferCreateInfo swapchain_framebuffer_create_info = {};
        swapchain_framebuffer_create_info.sType      = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        swapchain_framebuffer_create_info.renderPass = final_pass;
        swapchain_framebuffer_create_info.attachmentCount = ARRAY_LENGTH(attachments);
        swapchain_framebuffer_create_info.pAttachments    = attachments;
        swapchain_framebuffer_create_info.width           = app.swapchain_extent.width;
        swapchain_framebuffer_create_info.height          = app.swapchain_extent.height;
        swapchain_framebuffer_create_info.layers          = 1;

        VkFramebuffer new_framebuffer;
        VK_CHECK(vkCreateFramebuffer(app.device, &swapchain_framebuffer_create_info, nullptr,
                                     &new_framebuffer));

        swapchain_framebuffers.push_back(new_framebuffer);
    }

    // Create pipeline
    // This is some temp code pretty much ripped from the vulkan tutorial. This
    // will be filled from material config. We will know from this which pass
    // and subpass to use for a given pipeline.

    VkPipeline pipeline;
    {
        // Specify vertex layout. This will be provided by a config as well.
        // Application is responsible for making sure layouts match with
        // input buffers

        // Shader stages
        std::vector< VkPipelineShaderStageCreateInfo > shader_stages;
        for (const auto& module_data : shader_module_data) {
            VkPipelineShaderStageCreateInfo create_info = {};
            create_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            create_info.pName  = module_data.entry_point.c_str();
            create_info.module = module_data.module;
            create_info.stage  = module_data.stage;

            shader_stages.push_back(create_info);
        }

        // Vertex info
        // TODO: pull vertex info from vertex shader reflection data
        VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_info.vertexBindingDescriptionCount = 0;
        vertex_input_info.pVertexBindingDescriptions = nullptr;
        vertex_input_info.vertexAttributeDescriptionCount = 0;
        vertex_input_info.pVertexAttributeDescriptions = nullptr;

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        // Viewport and scissor
        VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = app.swapchain_extent.width;
        viewport.height = app.swapchain_extent.height;
        viewport.minDepth = 0;
        viewport.maxDepth = 1;

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = app.swapchain_extent;

        VkPipelineViewportStateCreateInfo viewport_state_info = {};
        viewport_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state_info.pViewports = &viewport;
        viewport_state_info.viewportCount = 1;
        viewport_state_info.pScissors = &scissor;
        viewport_state_info.scissorCount = 1;

        // Multisampling info
        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f;
        multisampling.pSampleMask = nullptr;
        multisampling.alphaToCoverageEnable = VK_FALSE;
        multisampling.alphaToOneEnable = VK_FALSE;

        // Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer_info = {};
        rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer_info.depthClampEnable = VK_FALSE;
        rasterizer_info.rasterizerDiscardEnable = VK_FALSE;
        rasterizer_info.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer_info.lineWidth = 1;
        rasterizer_info.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer_info.depthBiasClamp = VK_FALSE;
        rasterizer_info.depthBiasConstantFactor = 0.0f;
        rasterizer_info.depthBiasClamp = 0.0f;
        rasterizer_info.depthBiasSlopeFactor = 0.0f;

        // Depth and stencil buffer
        // TODO: Implement me!

        // Color blending
        // Default blending
        // TODO: This should come from pass configuration. The blend attachments correspond
        // with the pass attachments
        VkPipelineColorBlendAttachmentState color_blend_attachment = {};
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = VK_FALSE;
        color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo color_blending = {};
        color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.logicOp = VK_LOGIC_OP_COPY;
        color_blending.attachmentCount = 1;
        color_blending.pAttachments = &color_blend_attachment;
        color_blending.blendConstants[0] = 0.0f;
        color_blending.blendConstants[1] = 0.0f;
        color_blending.blendConstants[2] = 0.0f;
        color_blending.blendConstants[3] = 0.0f;

        VkDynamicState dynamic_states[] = {
            VK_DYNAMIC_STATE_VIEWPORT
        };

        VkPipelineDynamicStateCreateInfo dynamic_state = {};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = 1;
        dynamic_state.pDynamicStates = dynamic_states;

        // Pipeline
        VkGraphicsPipelineCreateInfo pipeline_create_info = {};
        pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_create_info.pStages = shader_stages.data();
        pipeline_create_info.stageCount = shader_stages.size();
        pipeline_create_info.pVertexInputState = &vertex_input_info;
        pipeline_create_info.pInputAssemblyState = &input_assembly;
        pipeline_create_info.pViewportState = &viewport_state_info;
        pipeline_create_info.pRasterizationState = &rasterizer_info;
        pipeline_create_info.pMultisampleState = &multisampling;
        pipeline_create_info.pDepthStencilState = nullptr;
        pipeline_create_info.pColorBlendState = &color_blending;
        pipeline_create_info.pDynamicState = &dynamic_state;

        pipeline_create_info.layout = pipeline_layout;
        pipeline_create_info.renderPass = final_pass;

        pipeline = renderer.request_pipeline(pipeline_create_info);
    }

    // TODO: Clear color is another per-attachment thing. This should be
    // pulled from pass config
    VkClearValue clear_color = {0.5f, 0.0f, 0.25f, 1.0f};
    auto render_func = [&app, &clear_color, &pipeline, &final_pass, &swapchain_framebuffers](const size_t image_index, VkCommandBuffer cmd_buf) {
        VkRenderPassBeginInfo final_pass_begin = {};
        final_pass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        final_pass_begin.renderPass = final_pass;
        final_pass_begin.framebuffer = swapchain_framebuffers[image_index];
        final_pass_begin.renderArea.offset = {0, 0};
        final_pass_begin.renderArea.extent = app.swapchain_extent;
        final_pass_begin.pClearValues = &clear_color;
        final_pass_begin.clearValueCount = 1;

        vkCmdBeginRenderPass(cmd_buf, &final_pass_begin, VK_SUBPASS_CONTENTS_INLINE);

        // Viewport and scissor
        VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = app.swapchain_extent.width;
        viewport.height = app.swapchain_extent.height;
        viewport.minDepth = 0;
        viewport.maxDepth = 1;

        vkCmdSetViewport(cmd_buf, 0, 1, &viewport);
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdDraw(cmd_buf, 3, 1, 0, 0);

        vkCmdEndRenderPass(cmd_buf);
    };

    // End temp code

    // Render loop
    while (!glfwWindowShouldClose(app.window)) {
        glfwPollEvents();
        Vulkan::render_frame(app, render_func);
    }

    // Clean up resources

    vkDestroyPipeline(app.device, pipeline, nullptr);
    
    vkDestroyRenderPass(app.device, final_pass, nullptr);
    for (auto& framebuffer : swapchain_framebuffers) {
        vkDestroyFramebuffer(app.device, framebuffer, nullptr);
    }

    // Deinit fs
    FileSystem::deinit();

    return 0;
}