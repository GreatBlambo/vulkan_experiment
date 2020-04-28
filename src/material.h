#pragma once

#include "vulkan_app.h"
#include "vulkan_utils.h"
#include "memory.h"
#include <limits>

#include <vector>

namespace Vulkan {

struct ResourceDescription {
    struct Component {
        VkFormat format;
        size_t offset;
    };

    struct Binding {
        size_t binding_number;
        Component* components;
        size_t num_components;
    };

    Binding* bindings;
    size_t num_bindings;
};

struct ShaderModuleData {
    VkShaderStageFlagBits stage;
    VkShaderModule module;

    struct InputAttribute {
        size_t location;
        VkFormat format;
    };
    std::vector< InputAttribute > inputs;

    struct DescriptorSetBinding {
        size_t set_number;
        VkDescriptorSetLayoutBinding binding;
    };
    std::vector< DescriptorSetBinding > set_bindings;

    std::string entry_point;
};

struct SPIRVModuleFileData {
    Memory::Buffer source_data;
    Memory::Buffer reflection_data;
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

class MaterialTable {
  private:
    Vulkan::App& app;
    VkPipelineCache pipeline_cache;

    std::vector< std::pair< VkShaderModuleCreateInfo, VkShaderModule > > shader_modules;
    std::vector< std::pair< VkDescriptorSetLayoutCreateInfo, VkDescriptorSetLayout > >
        descriptor_set_layouts;
    std::vector< std::pair< VkPipelineLayoutCreateInfo, VkPipelineLayout > > pipeline_layouts;

    VkDescriptorSetLayout
    request_descriptor_set_layout(const VkDescriptorSetLayoutCreateInfo& create_info);
    VkPipelineLayout request_pipeline_layout(const VkPipelineLayoutCreateInfo& create_info);
    VkShaderModule request_shader_module(const VkShaderModuleCreateInfo& create_info);

    VkPipelineLayout find_pipeline_layout(const VkPipelineLayoutCreateInfo& create_info);
    VkDescriptorSetLayout
    find_descriptor_set_layout(const VkDescriptorSetLayoutCreateInfo& create_info);
    VkShaderModule find_shader_module(const VkShaderModuleCreateInfo& create_info);

  public:
    MaterialTable(Vulkan::App& app);
    ~MaterialTable();

    void deserialize_module_data(const std::vector< SPIRVModuleFileData >& modules,
                                 std::vector< ShaderModuleData >& out_module_data);

    VkPipelineLayout
    create_pipeline_layout_from_modules(std::vector< ShaderModuleData >& shader_module_data);

    VkPipeline request_pipeline(const VkGraphicsPipelineCreateInfo& create_info);
};

}    // namespace Vulkan