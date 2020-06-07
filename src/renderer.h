#pragma once

#include "vulkan_app.h"
#include "vulkan_types.h"
#include "vulkan_utils.h"
#include "memory.h"
#include "renderer.h"

#include <limits>

#include <vector>

#include <parallel_hashmap/phmap.h>

namespace Vulkan {

// ID types

static const size_t RENDERER_INVALID_INDEX = (size_t) (~0);
static const size_t RENDERER_INVALID_OFFSET = (size_t) (~0);

struct ShaderModule {
    size_t index = RENDERER_INVALID_INDEX;
};

struct BufferLayout {
    struct Attribute {
        const char* name;
        Type type;
        size_t offset = RENDERER_INVALID_OFFSET;
    };

    struct Binding {
        size_t binding;
        Attribute* attributes;
        size_t num_attributes = 0;
        VkVertexInputRate input_rate = VK_VERTEX_INPUT_RATE_VERTEX;
    };

    Binding* bindings;
    size_t num_bindings = 0;
};

struct ShaderResourceCreateInfo {
    struct VertexInput {
        const char* name;
        VkFormat format;
    };

    VertexInput vertex_inputs[VULKAN_MAX_VERTEX_INPUTS];
    DescriptorBinding descriptor_bindings[VULKAN_MAX_DESCRIPTOR_SETS][VULKAN_MAX_DESCRIPTOR_BINDINGS];
};

struct ShaderModuleCreateInfo {
    const char* entry_point;
    VkShaderStageFlags stage;
    ShaderResourceCreateInfo resource_info;
};

class ResourceManager {
  public:
    ResourceManager(Vulkan::App& app, Memory::IAllocator& allocator);
    ~ResourceManager();

    // Get resource info
    const ShaderModuleCreateInfo* get_shader_module_info(const ShaderModule& module);

    // Member functions
    // Initialize shader module info from reflection json
    void deserialize_reflection_data(const Memory::Buffer& reflection_json,
                              ShaderModuleCreateInfo& out_reflection_data);

    // Request vulkan resources from cache. These functions will create
    // the resource if an identical one does not yet exist
    VkPipeline request_pipeline(const VkGraphicsPipelineCreateInfo& create_info);
    VkDescriptorSetLayout request_descriptor_set_layout(const DescriptorSetLayoutCreateInfo& create_info);
    VkPipelineLayout request_pipeline_layout(const PipelineLayoutCreateInfo& create_info);
    ShaderModule request_shader_module(const char* name, const Memory::Buffer& spirv_source,
                                      const Memory::Buffer& reflection_json);
    ShaderModule request_shader_module(const char* name, const Memory::Buffer& spirv_source,
                                      const ShaderModuleCreateInfo& create_info);

    // Find vulkan resources from cache
    VkPipelineLayout find_pipeline_layout(const VkPipelineLayoutCreateInfo& create_info);
    VkDescriptorSetLayout
    find_descriptor_set_layout(const VkDescriptorSetLayoutCreateInfo& create_info);
    VkShaderModule find_shader_module(const VkShaderModuleCreateInfo& create_info);

    const Vulkan::App& app();

  private:
    // Members
    Vulkan::App& m_app;

    // Caches
    phmap::flat_hash_map< DescriptorSetLayoutCreateInfo, VkDescriptorSetLayout >
        m_descriptor_set_layout_cache;
    phmap::flat_hash_map< PipelineLayoutCreateInfo, VkPipelineLayout > m_pipeline_layout_cache;
    VkPipelineCache m_pipeline_cache;

    // Indices
    phmap::flat_hash_map< const char*, ShaderModule, Hash::Hash<const char*> > m_name_to_shader_module;

    // Resource arrays
    std::vector< std::pair< ShaderModuleCreateInfo, VkShaderModule > > m_shader_modules;
    std::vector< VkPipeline > m_pipelines;

    // Allocators
    Memory::IAllocator& m_allocator;
    Memory::LinearAllocator m_string_allocator;
};

struct AttachmentCreateInfo {
    VkImageType image_type;
    VkFormat format;
    VkExtent3D extent;
    uint32_t mip_levels;
};

struct RenderPassCreateInfo {
    const char* name;

    const ShaderModule* shader_modules;
    size_t num_modules = 0;

    struct AttachmentBinding {
        size_t attachment_num;
        VkAttachmentDescription vk_attachment_description;
    }* attachment_bindings;
    size_t num_attachments = 0;

    // TODO: sorting and dependencies
};

struct RenderProgramConfig {
    AttachmentCreateInfo* attachments;
    size_t num_attachments = 0;

    RenderPassCreateInfo* render_passes;
    size_t num_render_passes;
};

class RenderProgram {
  public:
    RenderProgram(const RenderProgramConfig& config, 
                  ResourceManager& resource_manager);

    RenderProgram(const RenderProgram& other) = delete;
    RenderProgram& operator=(const RenderProgram& other) = delete;
    ~RenderProgram();
  private:
    struct Attachment {
        AttachmentCreateInfo info;
        VkImage vk_image;
        VkImageView vk_image_view;
    };

    struct RenderPass {
        // Default pipeline for  the render pass
        VkPipeline vk_pipeline;
        // Actual Render pass 
        VkRenderPass vk_render_pass;

        std::vector<RenderPassCreateInfo::AttachmentBinding> attachments;

        // TODO render jobs
    };

    void clean();
    void bake(const RenderProgramConfig& config);

    ResourceManager& m_resource_manager;

    std::vector<Attachment> m_attachments;
    std::vector<RenderPass> m_render_passes;
};

void render_frame(Vulkan::App& app, const std::function< void(const size_t, VkCommandBuffer) >& render);

}    // namespace Vulkan