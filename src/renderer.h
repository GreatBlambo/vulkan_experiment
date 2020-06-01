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

#define RENDERER_INVALID_INDEX (size_t) (~0)

struct ShaderModule {
    size_t index = RENDERER_INVALID_INDEX;
};

struct BufferLayout {
    struct Attribute {
        const char* name;
        Type type;
        size_t offset = std::numeric_limits<size_t>::max();
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

class Renderer {
  private:
    Vulkan::App& app;

    // Caches
    phmap::flat_hash_map< DescriptorSetLayoutCreateInfo, VkDescriptorSetLayout >
        descriptor_set_layout_cache;
    phmap::flat_hash_map< PipelineLayoutCreateInfo, VkPipelineLayout > pipeline_layout_cache;
    VkPipelineCache pipeline_cache;

    // Indices
    phmap::flat_hash_map< const char*, ShaderModule, Hash::Hash<const char*> > name_to_shader_module;

    // Resource arrays
    std::vector< std::pair< ShaderModuleCreateInfo, VkShaderModule > > shader_modules;
    std::vector< VkPipeline > pipelines;

    Memory::IAllocator& allocator;
    Memory::LinearAllocator string_allocator;

    // Initialize shader module info from reflection json
    void deserialize_reflection_data(const Memory::Buffer& reflection_json,
                              ShaderModuleCreateInfo& out_reflection_data);

    // Request vulkan resources from cache. These functions will create
    // the resource if an identical one does not yet exist
    VkPipeline request_pipeline(const VkGraphicsPipelineCreateInfo& create_info);
    VkDescriptorSetLayout request_descriptor_set_layout(const DescriptorSetLayoutCreateInfo& create_info);
    VkPipelineLayout request_pipeline_layout(const PipelineLayoutCreateInfo& create_info);

    // Find vulkan resources from cache
    VkPipelineLayout find_pipeline_layout(const VkPipelineLayoutCreateInfo& create_info);
    VkDescriptorSetLayout
    find_descriptor_set_layout(const VkDescriptorSetLayoutCreateInfo& create_info);
    VkShaderModule find_shader_module(const VkShaderModuleCreateInfo& create_info);
  public:
    Renderer(Vulkan::App& app, Memory::IAllocator& allocator);
    ~Renderer();

    // Get resource info
    const ShaderModuleCreateInfo* get_shader_module_info(const ShaderModule& module);

    ShaderModule create_shader_module(const char* name, const Memory::Buffer& spirv_source,
                                      const Memory::Buffer& reflection_json);
    ShaderModule create_shader_module(const char* name, const Memory::Buffer& spirv_source,
                                      const ShaderModuleCreateInfo& create_info);

};

void render_frame(Vulkan::App& app, const std::function< void(const size_t, VkCommandBuffer) >& render);

}    // namespace Vulkan