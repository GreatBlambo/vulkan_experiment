#pragma once

#include "vulkan_app.h"
#include "vulkan_utils.h"
#include "memory.h"
#include <limits>

#include <vector>

namespace Vulkan {

struct ShaderModuleData {
    VkShaderStageFlagBits stage;
    VkShaderModule module;

    struct InputAttribute {
        const char* name;
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

class MaterialSystem {
  private:
    Vulkan::App& app;
    VkPipelineCache pipeline_cache;

    std::vector< std::pair< VkShaderModuleCreateInfo, VkShaderModule > > shader_modules;
    std::vector< std::pair< VkDescriptorSetLayoutCreateInfo, VkDescriptorSetLayout > >
        descriptor_set_layouts;
    std::vector< std::pair< VkPipelineLayoutCreateInfo, VkPipelineLayout > > pipeline_layouts;


  public:
    struct ShaderModule {
        size_t index;
    };

    struct Material {
        size_t index;
    };

    struct ShaderResourceInfo {
        struct InputAttribute {
            const char* name;
            size_t location;
            VkFormat format;
        };
        Memory::Array<InputAttribute> inputs;

        struct DescriptorSetBinding {
            size_t set_number;
            VkDescriptorSetLayoutBinding binding;
        };
        Memory::Array<DescriptorSetBinding> set_bindings;
    };

    struct ShaderModuleInfo {
        const char* entry_point;
        VkShaderModule module;
        VkShaderStageFlags stage;
        ShaderResourceInfo resource_info;
    };

    struct MaterialInfo {
        struct Passes {
            Memory::Array<ShaderModule> shader_modules;
        };

        Memory::Array<Passes> passes;

        struct DescriptorSetLayout {
            size_t set_number;
            VkDescriptorSetLayout descriptor_set_layout;
        };
        Memory::Array<DescriptorSetLayout> descriptor_set_layouts;

        VkPipelineLayout pipeline_layout;
    };

    MaterialSystem(Vulkan::App& app);
    ~MaterialSystem();

    void deserialize_module_data(const std::vector< SPIRVModuleFileData >& modules,
                                 std::vector< ShaderModuleData >& out_module_data);

    ShaderModule create_shader_module(const char* name, const Memory::Buffer& spirv_source, const Memory::Buffer& reflection_json);
    ShaderModule create_shader_module(const char* name, const Memory::Buffer& spirv_source, VkShaderStageFlags stage, const ShaderResourceInfo& resources);
    const ShaderModuleInfo* get_shader_module_info(const ShaderModule& module);

    Material create_material(const char* name, const MaterialInfo& material_info);
    const MaterialInfo* get_material_info(const Material& material);

    // Request vulkan resources from cache. These functions will create
    // the resource if an identical one does not yet exist
    VkPipeline request_pipeline(const VkGraphicsPipelineCreateInfo& create_info);
    VkDescriptorSetLayout
    request_descriptor_set_layout(const VkDescriptorSetLayoutCreateInfo& create_info);
    VkPipelineLayout request_pipeline_layout(const VkPipelineLayoutCreateInfo& create_info);
    VkShaderModule request_shader_module(const VkShaderModuleCreateInfo& create_info);

    // Find vulkan resources from cache
    VkPipelineLayout find_pipeline_layout(const VkPipelineLayoutCreateInfo& create_info);
    VkDescriptorSetLayout
    find_descriptor_set_layout(const VkDescriptorSetLayoutCreateInfo& create_info);
    VkShaderModule find_shader_module(const VkShaderModuleCreateInfo& create_info);
};

}    // namespace Vulkan