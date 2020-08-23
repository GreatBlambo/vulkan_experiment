#pragma once

#include "vulkan_app.h"
#include "vulkan_types.h"
#include "vulkan_utils.h"
#include "memory.h"

#include <limits>

#include <vector>

#include <parallel_hashmap/phmap.h>

namespace Vulkan {

// ID types

static const size_t HANDLE_INVALID_INDEX = (size_t) (~0);
static const size_t INVALID_OFFSET = (size_t) (~0);

template <typename PHANTOM_T>
struct Handle {
    size_t index = HANDLE_INVALID_INDEX;
    inline bool is_valid() const {
        return index != HANDLE_INVALID_INDEX;
    }
    inline static Handle<PHANTOM_T> create_invalid() {
        return { HANDLE_INVALID_INDEX };
    }
};

typedef Handle<struct ShaderModule_T> ShaderModule;

struct BufferLayout {
    struct Attribute {
        const char* name;
        Type type;
        size_t offset = INVALID_OFFSET;
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

class ResourceManager {
  public:
    ResourceManager(Vulkan::App& app, Memory::IAllocator& allocator);
    ~ResourceManager();

    // Get handles by name
    ShaderModule find_shader_module(const char* name);

    // Get resource info
    const ShaderModuleCreateInfo& get_shader_module_info(const ShaderModule& module);
    VkShaderModule get_shader_module(const ShaderModule& module);

    // Member functions
    // Initialize shader module info from reflection json
    void deserialize_reflection_data(const Memory::Buffer& reflection_json,
                              ShaderModuleCreateInfo& out_reflection_data);

    // Request vulkan resources from cache. These functions will create
    // the resource if an identical one does not yet exist
    VkPipeline request_pipeline(const VkGraphicsPipelineCreateInfo& create_info);
    VkDescriptorSetLayout request_descriptor_set_layout(const DescriptorSetLayoutCreateInfo& create_info);
    VkPipelineLayout request_pipeline_layout(const PipelineLayoutCreateInfo& create_info);
    ShaderModule request_shader_module(const ShaderSource& shader_source);
    ShaderModule request_shader_module(const char* name, const Memory::Buffer& spirv_source,
                                      const ShaderModuleCreateInfo& create_info);

    // Find vulkan resources from cache
    VkPipelineLayout find_pipeline_layout(const VkPipelineLayoutCreateInfo& create_info);
    VkDescriptorSetLayout
    find_descriptor_set_layout(const VkDescriptorSetLayoutCreateInfo& create_info);
    VkShaderModule find_shader_module(const VkShaderModuleCreateInfo& create_info);

    const Vulkan::App& app();

    // Clears all resources
    void clear();

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


// Resource manager helpers
VkPipelineLayout create_pipeline_layout(ResourceManager& resource_manager, const std::vector<ShaderModule>& shader_modules);


}    // namespace Vulkan