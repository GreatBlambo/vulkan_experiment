#include "vulkan_app.h"
#include "vulkan_utils.h"

#include "utils.h"

#include "vulkan_resource_manager.h"

#include <array>
#include <algorithm>
#include <unordered_map>

#include <rapidjson/document.h>
#include <rapidjson/schema.h>

#include <glm/glm.hpp>

#include <static/static_resources.h>

namespace Vulkan {

/////////////////////////////////////////////////////////////////////////////////////////////////
// Material Resources ///////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////

// Material resources include any vulkan objects required to make a material.
// The intention here is to allow the user to string together configurations on the fly and we will
// reuse vulkan objects as necessary.

static VkVertexInputRate get_vertex_input_rate(const char* str) {
    unsigned long hash = Hash::djb2_hash(str);
    switch (hash) {
        case Hash::djb2_hash("vertex"): return VK_VERTEX_INPUT_RATE_VERTEX;
        case Hash::djb2_hash("instance"): return VK_VERTEX_INPUT_RATE_INSTANCE;
        default: RUNTIME_ERROR("Unknown input rate %s", str);
    }
}

static void validate_type(rapidjson::GenericObject< false, rapidjson::Value >& types,
                          const char* type_name) {
    ASSERT(types.HasMember(type_name));
    ASSERT(types[type_name].IsObject());

    auto type = types[type_name].GetObject();
    ASSERT(type.HasMember("name"));
    ASSERT(type["name"].IsString());
    ASSERT(type.HasMember("members"));
    ASSERT(type["members"].IsArray());

    auto members = type["members"].GetArray();
    for (size_t i = 0; i < members.Size(); i++) {
        auto& member = members[i];
        ASSERT(member.HasMember("name"));
        ASSERT(member["name"].IsString());
        ASSERT(member.HasMember("type"));
        ASSERT(member["type"].IsString());
        ASSERT(member.HasMember("offset"));
        ASSERT(member["offset"].IsInt());
    }
}

static TypeInfo get_type_info(const char* type_name,
                              rapidjson::GenericObject< false, rapidjson::Value >* types
                              = nullptr) {
    unsigned long hash = Hash::djb2_hash(type_name);
    switch (hash) {
        case Hash::djb2_hash("float"): return get_type_info(Type::FLOAT);
        case Hash::djb2_hash("vec2"): return get_type_info(Type::VEC2);
        case Hash::djb2_hash("vec3"): return get_type_info(Type::VEC3);
        case Hash::djb2_hash("vec4"): return get_type_info(Type::VEC4);
        case Hash::djb2_hash("mat2"): return get_type_info(Type::MAT2);
        case Hash::djb2_hash("mat3"): return get_type_info(Type::MAT3);
        case Hash::djb2_hash("mat4"): return get_type_info(Type::MAT4);
        default:
            if (types) {
                auto& types_obj = *types;
                validate_type(types_obj, type_name);
                auto type    = types_obj[type_name].GetObject();
                auto members = type["members"].GetArray();

                TypeInfo type_info = {};
                for (size_t i = 0; i < members.Size(); i++) {
                    auto& member              = members[i];
                    TypeInfo member_type_info = get_type_info(member["type"].GetString(), types);

                    type_info.data_size += member["offset"].GetInt() + member_type_info.data_size;
                    type_info.location_span += member_type_info.location_span;
                }
                return type_info;
            } else {
                RUNTIME_ERROR("Unknown type %s", type_name);
            }
    }
}

static VkDescriptorType get_descriptor_type(const char* node_name, const char* type) {
    VkDescriptorType result = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    unsigned long node_hash = Hash::djb2_hash(node_name);
    if (node_hash == Hash::djb2_hash("ubos")) {
        result = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    } else if (node_hash == Hash::djb2_hash("textures")) {
        unsigned long type_hash = Hash::djb2_hash(type);
        if (type_hash == Hash::djb2_hash("sampler2D")) {
            result = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        }
    }

    ASSERT_MSG(result != VK_DESCRIPTOR_TYPE_MAX_ENUM, "Unsupported node %s, type %s", node_name,
               type);
    return result;
}

ShaderModule ResourceManager::find_shader_module(const char* name) {
    auto module_it = m_name_to_shader_module.find(name);
    if (module_it != m_name_to_shader_module.end()) {
        return module_it->second;
    }
    return ShaderModule::create_invalid();
}

const ShaderModuleCreateInfo& ResourceManager::get_shader_module_info(const ShaderModule& shader_module) {
    ASSERT_MSG(shader_module.is_valid() && shader_module.index < m_shader_modules.size(), "Invalid shader module handle %lu", shader_module.index);
    return m_shader_modules[shader_module.index].first;
}

VkShaderModule ResourceManager::get_shader_module(const ShaderModule& shader_module) {
    ASSERT_MSG(shader_module.is_valid() && shader_module.index < m_shader_modules.size(), "Invalid shader module handle %lu", shader_module.index);
    return m_shader_modules[shader_module.index].second;
}

void ResourceManager::deserialize_reflection_data(const Memory::Buffer& reflection_json,
                                           ShaderModuleCreateInfo& out_reflection_data) {
    rapidjson::Document document;
    document.Parse((const char*) reflection_json.data, reflection_json.size);

    ASSERT(document.IsObject());
    auto root = document.GetObject();

    ASSERT(root.HasMember("entryPoints"));
    ASSERT(root["entryPoints"].IsArray());
    auto entry_points = root["entryPoints"].GetArray();

    ASSERT_MSG(entry_points.Size() <= 2, "Multiple entry points is currently not supported.");
    ASSERT_MSG(entry_points.Size() > 0, "No entry point supplied.");

    ASSERT(entry_points[0].IsObject());
    auto entry_point = entry_points[0].GetObject();

    ASSERT(entry_point.HasMember("name"));
    ASSERT(entry_point["name"].IsString());
    out_reflection_data.entry_point = m_string_allocator.copy_string(entry_point["name"].GetString());

    ASSERT(entry_point.HasMember("mode"));
    ASSERT(entry_point["mode"].IsString());
    auto mode = entry_point["mode"].GetString();

    if (strcmp("vert", mode) == 0) {
        out_reflection_data.stage = VK_SHADER_STAGE_VERTEX_BIT;
        if (root.HasMember("inputs")) {
            ASSERT(root["inputs"].IsArray());
            auto inputs = root["inputs"].GetArray();

            ASSERT(inputs.Size() <= VULKAN_MAX_VERTEX_INPUTS);
            for (size_t i = 0; i < inputs.Size(); i++) {
                ASSERT(inputs[i].IsObject());
                auto input = inputs[i].GetObject();

                ASSERT(input.HasMember("type"));
                ASSERT(input["type"].IsString());
                ASSERT(input.HasMember("location"));
                ASSERT(input["location"].IsInt());
                ASSERT(input.HasMember("name"));
                ASSERT(input["name"].IsString());

                size_t location = input["location"].GetInt();
                ASSERT(location < VULKAN_MAX_VERTEX_INPUTS);

                ShaderResourceCreateInfo::VertexInput& input_attrib
                    = out_reflection_data.resource_info.vertex_inputs[location];
                input_attrib.format = get_type_info(input["type"].GetString()).format;
                input_attrib.name   = m_allocator.copy_string(input["name"].GetString());
            }
        }
    } else if (strcmp("frag", mode) == 0) {
        out_reflection_data.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    } else if (strcmp("geom", mode) == 0) {
        out_reflection_data.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    } else if (strcmp("tesc", mode) == 0) {
        out_reflection_data.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    } else if (strcmp("tese", mode) == 0) {
        out_reflection_data.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    }

    auto& string_allocator                 = this->m_string_allocator;
    const auto add_descriptor_set_bindings = [&out_reflection_data, &root,
                                              &string_allocator](const char* node_name) {
        if (root.HasMember(node_name)) {
            ASSERT(root[node_name].IsArray());
            auto array = root[node_name].GetArray();

            for (size_t i = 0; i < array.Size(); i++) {
                ASSERT(array[i].IsObject());
                auto binding_obj = array[i].GetObject();

                ASSERT(binding_obj.HasMember("type"));
                ASSERT(binding_obj["type"].IsString());
                ASSERT(binding_obj.HasMember("set"));
                ASSERT(binding_obj["set"].IsInt());
                ASSERT(binding_obj.HasMember("binding"));
                ASSERT(binding_obj["binding"].IsInt());
                ASSERT(binding_obj.HasMember("name"));
                ASSERT(binding_obj["name"].IsString());

                size_t set_number     = binding_obj["set"].GetInt();
                size_t binding_number = binding_obj["binding"].GetInt();

                ASSERT(set_number < VULKAN_MAX_DESCRIPTOR_SETS);
                ASSERT(binding_number < VULKAN_MAX_DESCRIPTOR_BINDINGS);

                DescriptorBinding& binding_info
                    = out_reflection_data.resource_info
                          .descriptor_bindings[set_number][binding_number];

                if (binding_obj.HasMember("array")) {
                    ASSERT(binding_obj["array"].IsArray());
                    auto array_member = binding_obj["array"].GetArray();
                    ASSERT_MSG(array_member.Size() == 0, "Only single dimension arrays supported");
                    ASSERT(array_member[0].IsInt());

                    binding_info.descriptor_count = array_member[0].GetInt();
                }

                binding_info.stage_flags = out_reflection_data.stage;
                binding_info.descriptor_type
                    = get_descriptor_type(node_name, binding_obj["type"].GetString());
                binding_info.name = string_allocator.copy_string(binding_obj["name"].GetString());
            }
        }
    };

    add_descriptor_set_bindings("textures");
    add_descriptor_set_bindings("ubos");

    // TODO: other descriptor types

    // TODO: Extract push constants
    /*
    if (root.HasMember("push_constants")) {
        ASSERT(root.HasMember("types"));
        ASSERT(root["types"].IsObject());
        auto& types = root["types"].GetObject();

        ASSERT(root["push_constants"].IsArray());
        auto& array = root["push_constants"].GetArray();

        size_t current_offset = 0;

        for (size_t i = 0; i < array.Size(); i++) {
            ASSERT(array[i].IsObject());
            auto& push_constant = array[i].GetObject();

            ASSERT(push_constant.HasMember("type"));
            ASSERT(push_constant["type"].IsString());
            ASSERT(push_constant.HasMember("name"));
            ASSERT(push_constant["name"].IsString());

            const char* type_name = push_constant["name"].GetString();
            TypeInfo type_info = get_type_info(type_name, &types);

            out_reflection_data.push_constant_ranges.push_back(new_range);
        }
    }
    */
}

static BufferLayout* get_default_mesh_layout() {
    static BufferLayout::Attribute per_vertex_attributes[]
        = { { "position", Type::VEC3 }, { "normal", Type::VEC3 }, { "uv", Type::VEC2 } };

    static BufferLayout::Attribute per_instance_attributes[] = { { "mvm", Type::MAT4 } };

    static BufferLayout::Binding bindings[]
        = { { 0, per_vertex_attributes, ARRAY_LENGTH(per_vertex_attributes) },
            { 1, per_instance_attributes, ARRAY_LENGTH(per_instance_attributes),
              VK_VERTEX_INPUT_RATE_INSTANCE } };

    static BufferLayout layout = { bindings, ARRAY_LENGTH(bindings) };

    return &layout;
}

ResourceManager::ResourceManager(Vulkan::App& app, Memory::IAllocator& allocator)
    : m_app(app)
    , m_allocator(allocator)
    , m_string_allocator(KB(10), allocator) {
    VkPipelineCacheCreateInfo pipeline_cache_create_info = {};
    pipeline_cache_create_info.sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    pipeline_cache_create_info.initialDataSize = 0;
    VK_CHECK(vkCreatePipelineCache(m_app.device, &pipeline_cache_create_info, nullptr,
                                   &this->m_pipeline_cache));
}

ResourceManager::~ResourceManager() {
    clear();
    vkDestroyPipelineCache(m_app.device, m_pipeline_cache, nullptr);
    m_string_allocator.release();
}

void ResourceManager::clear() {
    for (const auto& shader_module : m_shader_modules) {
        vkDestroyShaderModule(m_app.device, shader_module.second, nullptr);
    }

    for (const auto& descriptor_set_layout : m_descriptor_set_layout_cache) {
        vkDestroyDescriptorSetLayout(m_app.device, descriptor_set_layout.second, nullptr);
    }

    for (const auto& pipeline_layout : m_pipeline_layout_cache) {
        vkDestroyPipelineLayout(m_app.device, pipeline_layout.second, nullptr);
    }

    for (const auto& pipeline : m_pipelines) {
        vkDestroyPipeline(m_app.device, pipeline, nullptr);
    }

    m_string_allocator.clear();
}

ShaderModule ResourceManager::request_shader_module(const char* name, const Memory::Buffer& spirv_source,
                                            const ShaderModuleCreateInfo& create_info) {
    VkShaderModuleCreateInfo vk_create_info = {};
    vk_create_info.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vk_create_info.codeSize                 = spirv_source.size;
    vk_create_info.pCode                    = (uint32_t*) spirv_source.data;

    VkShaderModule vk_module;
    VK_CHECK(vkCreateShaderModule(m_app.device, &vk_create_info, nullptr, &vk_module));

    m_shader_modules.push_back(
        std::pair< ShaderModuleCreateInfo, VkShaderModule >(create_info, vk_module));

    ShaderModule id = { m_shader_modules.size() - 1 };

    m_name_to_shader_module[name] = id;
    return id;
}

ShaderModule ResourceManager::request_shader_module(const ShaderSource& shader_source) {
    ShaderModuleCreateInfo new_shader_module;
    deserialize_reflection_data(shader_source.reflection_json, new_shader_module);

    return request_shader_module(shader_source.name, shader_source.spirv_source, new_shader_module);
}

VkPipeline ResourceManager::request_pipeline(const VkGraphicsPipelineCreateInfo& create_info) {
    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(m_app.device, this->m_pipeline_cache, 1, &create_info, nullptr,
                                       &pipeline));
    m_pipelines.push_back(pipeline);
    return pipeline;
}

VkDescriptorSetLayout
ResourceManager::request_descriptor_set_layout(const DescriptorSetLayoutCreateInfo& create_info) {
    auto it = m_descriptor_set_layout_cache.find(create_info);
    if (it != m_descriptor_set_layout_cache.end()) {
        return it->second;
    }

    VkDescriptorSetLayoutBinding bindings[VULKAN_MAX_DESCRIPTOR_BINDINGS];

    VkDescriptorSetLayoutCreateInfo vk_create_info = {};
    vk_create_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    vk_create_info.pBindings    = bindings;
    vk_create_info.bindingCount = 0;

    for (size_t i = 0; i < VULKAN_MAX_DESCRIPTOR_BINDINGS; i++) {
        if (create_info.bindings[i].stage_flags == 0) {
            continue;
        }

        bindings[vk_create_info.bindingCount].binding = 0;
        bindings[vk_create_info.bindingCount].descriptorCount
            = create_info.bindings[i].descriptor_count;
        bindings[vk_create_info.bindingCount].descriptorType
            = create_info.bindings[i].descriptor_type;

        vk_create_info.bindingCount++;
    }

    VkDescriptorSetLayout set_layout;
    VK_CHECK(vkCreateDescriptorSetLayout(m_app.device, &vk_create_info, nullptr, &set_layout));

    m_descriptor_set_layout_cache[create_info] = set_layout;
    return set_layout;
}

VkPipelineLayout ResourceManager::request_pipeline_layout(const PipelineLayoutCreateInfo& create_info) {
    auto it = m_pipeline_layout_cache.find(create_info);
    if (it != m_pipeline_layout_cache.end()) {
        return it->second;
    }

    VkDescriptorSetLayout set_layouts[VULKAN_MAX_DESCRIPTOR_SETS];

    VkPipelineLayoutCreateInfo vk_create_info = {};
    vk_create_info.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (create_info.push_constant_range.size == 0) {
        vk_create_info.pPushConstantRanges = &create_info.push_constant_range;
    } else {
        vk_create_info.pPushConstantRanges = nullptr;
    }
    vk_create_info.pushConstantRangeCount     = 1;
    vk_create_info.pSetLayouts                = set_layouts;
    vk_create_info.setLayoutCount             = 0;

    for (size_t i = 0; i < VULKAN_MAX_DESCRIPTOR_SETS; i++) {
        if (create_info.descriptor_set_layouts[i] == VK_NULL_HANDLE) {
            continue;
        }

        set_layouts[vk_create_info.setLayoutCount] = create_info.descriptor_set_layouts[i];
        vk_create_info.setLayoutCount++;
    }

    VkPipelineLayout pipeline_layout;
    VK_CHECK(vkCreatePipelineLayout(m_app.device, &vk_create_info, nullptr, &pipeline_layout));

    m_pipeline_layout_cache[create_info] = pipeline_layout;
    return pipeline_layout;
}

const App& ResourceManager::app() {
    return m_app;
}

VkPipelineLayout create_pipeline_layout(
    ResourceManager& resource_manager,
    const std::vector<ShaderModule>& shader_modules
) {
    PipelineLayoutCreateInfo create_info;

    // Attempt to merge shader resource requirements into one cohesive collection of sets and bindings
    DescriptorBinding descriptor_bindings[VULKAN_MAX_DESCRIPTOR_SETS][VULKAN_MAX_DESCRIPTOR_BINDINGS];
    for (auto& module : shader_modules) {
        const ShaderResourceCreateInfo& module_resources = resource_manager.get_shader_module_info(module).resource_info;

        for (int set = 0; set < VULKAN_MAX_DESCRIPTOR_SETS; set++) {
            DescriptorSetLayoutCreateInfo desc_layout_create_info;
            for (int binding = 0; binding < VULKAN_MAX_DESCRIPTOR_BINDINGS; binding++) {
                const DescriptorBinding& this_binding = module_resources.descriptor_bindings[set][binding];
                const DescriptorBinding& current_binding = module_resources.descriptor_bindings[set][binding];
                // If the binding is empty, skip
                if (this_binding.stage_flags == 0) {
                    continue;
                }
                // If the current binding is not initialized, assign this binding to it
                else if (current_binding.stage_flags == 0) {
                    descriptor_bindings[set][binding] = this_binding;
                }
                // If the current binding is initialized, check if it's equal. If so, continue. If not, error
                else if (current_binding == this_binding) {
                    continue;
                } else {
                    RUNTIME_ERROR("Descriptor binding collision in shader %s, at set %d, binding %");
                }
            }
        }
    }

    for (int set = 0; set < VULKAN_MAX_DESCRIPTOR_SETS; set++) {
        DescriptorSetLayoutCreateInfo desc_set_layout_create_info;
        bool is_empty = true;
        for (int binding = 0; binding < VULKAN_MAX_DESCRIPTOR_BINDINGS; binding++) {
            const DescriptorBinding& this_binding = descriptor_bindings[set][binding];
            if (this_binding.stage_flags == 0) {
                continue;
            }
            desc_set_layout_create_info.bindings[binding] = descriptor_bindings[set][binding];

            // If there's a binding in this set, it's not empty
            is_empty = false;
        }

        // If the set is empty, skip it
        if (is_empty) {
            continue;
        }
        VkDescriptorSetLayout desc_set_layout = resource_manager.request_descriptor_set_layout(desc_set_layout_create_info);
        create_info.descriptor_set_layouts[set] = desc_set_layout;
    }

    return resource_manager.request_pipeline_layout(create_info);
}

}    // namespace Vulkan