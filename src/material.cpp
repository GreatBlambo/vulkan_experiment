#include "vulkan_app.h"
#include "vulkan_utils.h"

#include "utils.h"

#include "material.h"

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
    unsigned long hash = Utils::djb2_hash(str);
    switch (hash) {
        case Utils::djb2_hash("vertex"): return VK_VERTEX_INPUT_RATE_VERTEX;
        case Utils::djb2_hash("instance"): return VK_VERTEX_INPUT_RATE_INSTANCE;
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
    unsigned long hash = Utils::djb2_hash(type_name);
    switch (hash) {
        case Utils::djb2_hash("float"): return get_type_info(Type::FLOAT);
        case Utils::djb2_hash("vec2"): return get_type_info(Type::VEC2);
        case Utils::djb2_hash("vec3"): return get_type_info(Type::VEC3);
        case Utils::djb2_hash("vec4"): return get_type_info(Type::VEC4);
        case Utils::djb2_hash("mat2"): return get_type_info(Type::MAT2);
        case Utils::djb2_hash("mat3"): return get_type_info(Type::MAT3);
        case Utils::djb2_hash("mat4"): return get_type_info(Type::MAT4);
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

/*
static void deserialize_vertex_layout_data(const Memory::Buffer& layout_json,
                                    VertexLayoutData& out_layout_data) {
    // Deserialize schema
    rapidjson::Document schema_doc;
    size_t schema_data_size = 0;
    const char* schema_data
        = StaticResource::accessor("vertex_layout_schema.json", &schema_data_size);
    ASSERT(schema_data_size > 0);
    ASSERT(schema_data != nullptr);
    ASSERT(!schema_doc.Parse(schema_data, schema_data_size).HasParseError());

    rapidjson::SchemaDocument schema(schema_doc);
    rapidjson::SchemaValidator validator(schema);

    // Parse layout json
    rapidjson::Document layout_doc;
    ASSERT(!layout_doc.Parse((const char*) layout_json.data, layout_json.size).HasParseError());

    // Validate json confirms to schema
    if (!layout_doc.Accept(validator)) {
        rapidjson::StringBuffer sb;
        validator.GetInvalidSchemaPointer().StringifyUriFragment(sb);
        LOG_ERROR("Invalid object: %s", sb.GetString());
        LOG_ERROR("Invalid attribute: %s", validator.GetInvalidSchemaKeyword());
        sb.Clear();
        validator.GetInvalidDocumentPointer().StringifyUriFragment(sb);
        RUNTIME_ERROR("Invalid document: %s", sb.GetString());
    }

    // Load values from json
    auto root = layout_doc.GetObject();

    auto bindings = root["bindings"].GetArray();
    for (size_t i = 0; i < bindings.Size(); i++) {
        auto binding = bindings[i].GetObject();

        VkVertexInputBindingDescription binding_desc = {};
        binding_desc.binding                         = binding["binding"].GetInt();
        binding_desc.inputRate = get_vertex_input_rate(binding["input_rate"].GetString());
        binding_desc.stride    = 0;

        size_t current_stride = 0;

        auto attributes = binding["attributes"].GetArray();
        for (size_t j = 0; j < attributes.Size(); j++) {
            auto attribute = attributes[j].GetObject();

            TypeInfo type_info = get_type_info(attribute["type"].GetString());

            VertexLayoutData::AttributeDescription attribute_desc;
            attribute_desc.name   = attribute["name"].GetString();
            attribute_desc.format = type_info.format;
            attribute_desc.offset
                = attribute.HasMember("offset") ? attribute["offset"].GetInt() : current_stride;
            attribute_desc.binding = binding_desc.binding;

            ASSERT(attribute_desc.offset >= current_stride);

            current_stride = attribute_desc.offset + type_info.data_size;

            out_layout_data.input_attributes.push_back(attribute_desc);
        }

        binding_desc.stride = current_stride;
        out_layout_data.input_bindings.push_back(binding_desc);
    }
}
*/

static VkDescriptorType get_descriptor_type(const char* node_name, const char* type) {
    VkDescriptorType result = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    if (strcmp("ubos", node_name)) {
        result = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    } else if (strcmp("textures", node_name)) {
        if (strcmp("sampler2D", type)) {
            result = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        }
    }

    ASSERT_MSG(result != VK_DESCRIPTOR_TYPE_MAX_ENUM, "Unsupported node %s, type %s", node_name,
               type);
    return result;
}

static void load_reflection_data(const Memory::Buffer& reflection_json,
                          ShaderModuleData& out_reflection_data) {
    rapidjson::Document document;
    document.Parse((const char*) reflection_json.data, reflection_json.size);

    ASSERT(document.IsObject());
    auto root = document.GetObject();

    ASSERT(root.HasMember("entryPoints"));
    ASSERT(root["entryPoints"].IsArray());
    auto entry_points = root["entryPoints"].GetArray();

    ASSERT_MSG(entry_points.Size() < 2, "Multiple entry points is currently not supported.");
    ASSERT_MSG(entry_points.Size() > 0, "No entry point supplied.");

    ASSERT(entry_points[0].IsObject());
    auto entry_point = entry_points[0].GetObject();

    ASSERT(entry_point.HasMember("name"));
    ASSERT(entry_point["name"].IsString());
    out_reflection_data.entry_point = entry_point["name"].GetString();

    ASSERT(entry_point.HasMember("mode"));
    ASSERT(entry_point["mode"].IsString());
    auto mode = entry_point["mode"].GetString();

    if (strcmp("vert", mode) == 0) {
        out_reflection_data.stage = VK_SHADER_STAGE_VERTEX_BIT;
        if (root.HasMember("inputs")) {
            ASSERT(root["inputs"].IsArray());
            auto inputs = root["inputs"].GetArray();

            for (size_t i = 0; i < inputs.Size(); i++) {
                ASSERT(inputs[i].IsObject());
                auto input = inputs[i].GetObject();

                ASSERT(input.HasMember("type"));
                ASSERT(input["type"].IsString());
                ASSERT(input.HasMember("location"));
                ASSERT(input["location"].IsInt());

                ShaderModuleData::InputAttribute input_attrib;
                input_attrib.format   = get_type_info(input["type"].GetString()).format;
                input_attrib.location = input["location"].GetInt();

                out_reflection_data.inputs.push_back(input_attrib);
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

    const auto add_descriptor_set_bindings = [&out_reflection_data, &root](const char* node_name) {
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

                ShaderModuleData::DescriptorSetBinding set_bindings;
                set_bindings.set_number = binding_obj["set"].GetInt();

                set_bindings.binding         = {};
                set_bindings.binding.binding = binding_obj["binding"].GetInt();

                if (binding_obj.HasMember("array")) {
                    ASSERT(binding_obj["array"].IsArray());
                    auto array_member = binding_obj["array"].GetArray();
                    ASSERT_MSG(array_member.Size() == 0, "Only single dimension arrays supported");
                    ASSERT(array_member[0].IsInt());

                    set_bindings.binding.descriptorCount = array_member[0].GetInt();
                }
                set_bindings.binding.stageFlags = out_reflection_data.stage;
                set_bindings.binding.descriptorType
                    = get_descriptor_type(node_name, binding_obj["type"].GetString());
                set_bindings.binding.pImmutableSamplers = nullptr;

                out_reflection_data.set_bindings.push_back(set_bindings);
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

void MaterialTable::deserialize_module_data(const std::vector< SPIRVModuleFileData >& modules,
                             std::vector< ShaderModuleData >& out_module_data) {
    out_module_data.resize(modules.size());
    for (size_t i = 0; i < modules.size(); i++) {
        // Parse reflection data
        load_reflection_data(modules[i].reflection_data, out_module_data[i]);

        // Create shader module
        VkShaderModuleCreateInfo module_create_info = {};
        module_create_info.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        module_create_info.pCode                    = (uint32_t*) modules[i].source_data.data;
        module_create_info.codeSize                 = modules[i].source_data.size;

        out_module_data[i].module = request_shader_module(module_create_info);
    }
}

static BufferLayout* get_default_resource_layout() {
    static BufferLayout::Attribute per_vertex_attributes[] = {
        {
            "position",
            Type::VEC3
        },
        {
            "normal",
            Type::VEC3
        },
        {
            "uv",
            Type::VEC2
        }
    };

    static BufferLayout::Attribute per_instance_attributes[] = {
        {
            "mvm",
            Type::MAT4
        }
    };

    static BufferLayout::Binding bindings[] = {
        {
            0,
            per_vertex_attributes,
            ARRAY_LENGTH(per_vertex_attributes)
        },
        {
            1,
            per_instance_attributes,
            ARRAY_LENGTH(per_instance_attributes),
            VK_VERTEX_INPUT_RATE_INSTANCE
        }
    };

    static BufferLayout layout = {
        bindings,
        ARRAY_LENGTH(bindings)
    };

    return &layout;
}

MaterialTable::MaterialTable(Vulkan::App& app) 
    : app(app) {
    VkPipelineCacheCreateInfo pipeline_cache_create_info = {};
    pipeline_cache_create_info.sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    pipeline_cache_create_info.initialDataSize = 0;
    VK_CHECK(vkCreatePipelineCache(app.device, &pipeline_cache_create_info, nullptr,
                                   &this->pipeline_cache));
}

MaterialTable::~MaterialTable() {
    for (const auto& shader_module : shader_modules) {
        delete[] shader_module.first.pCode;
        vkDestroyShaderModule(app.device, shader_module.second, nullptr);
    }

    for (const auto& descriptor_set_layout : descriptor_set_layouts) {
        if (descriptor_set_layout.first.bindingCount > 0) {
            delete[] descriptor_set_layout.first.pBindings;
        }
        vkDestroyDescriptorSetLayout(app.device, descriptor_set_layout.second,
                                     nullptr);
    }

    for (const auto& pipeline_layout : pipeline_layouts) {
        if (pipeline_layout.first.pushConstantRangeCount > 0) {
            delete[] pipeline_layout.first.pPushConstantRanges;
        }
        if (pipeline_layout.first.setLayoutCount > 0) {
            delete[] pipeline_layout.first.pSetLayouts;
        }
        vkDestroyPipelineLayout(app.device, pipeline_layout.second, nullptr);
    }

    vkDestroyPipelineCache(app.device, pipeline_cache, nullptr);
}

// We can do these find_* functions even with create info structs which contain non-dispatchable
// handles because in all likelihood if two handles have the same value, the implementation believes
// them to be equivalent. So they aren't really collisions, in this case.

VkShaderModule MaterialTable::find_shader_module(const VkShaderModuleCreateInfo& create_info) {
    // Find module
    VkShaderModule result = VK_NULL_HANDLE;
    for (const auto& shader_module : shader_modules) {
        const VkShaderModuleCreateInfo& existing_create_info = shader_module.first;
        const VkShaderModule& existing_shader_module         = shader_module.second;
        if (existing_create_info.codeSize != create_info.codeSize) {
            continue;
        }
        if (memcmp(existing_create_info.pCode, create_info.pCode, create_info.codeSize) != 0) {
            continue;
        }
        result = existing_shader_module;
    }
    return result;
}

bool operator==(const VkDescriptorSetLayoutBinding& l, const VkDescriptorSetLayoutBinding& r) {
    bool immutable_samplers_same = false;
    if (l.pImmutableSamplers == r.pImmutableSamplers && l.pImmutableSamplers == nullptr) {
        immutable_samplers_same = true;
    } else if (l.descriptorCount == r.descriptorCount) {
        IMPLEMENT_ME_DESC("Immutable samplers");
    }
    return l.binding == r.binding && l.descriptorCount == r.descriptorCount
           && l.descriptorType == r.descriptorType && l.stageFlags == r.stageFlags
           && immutable_samplers_same;
}

bool operator!=(const VkDescriptorSetLayoutBinding& l, const VkDescriptorSetLayoutBinding& r) {
    return !(l == r);
}

VkDescriptorSetLayout
MaterialTable::find_descriptor_set_layout(const VkDescriptorSetLayoutCreateInfo& create_info) {
    // This function assumes that descriptor set layout bindings are sorted by
    // binding #
    // Find descriptor set layout
    VkDescriptorSetLayout result = VK_NULL_HANDLE;
    for (const auto& descriptor_set_layout : descriptor_set_layouts) {
        const VkDescriptorSetLayoutCreateInfo& existing_create_info = descriptor_set_layout.first;
        const VkDescriptorSetLayout& existing_descriptor_set_layout = descriptor_set_layout.second;

        if (existing_create_info.bindingCount != create_info.bindingCount) {
            continue;
        }

        bool bindings_same = true;
        for (size_t i = 0; i < existing_create_info.bindingCount; i++) {
            if (existing_create_info.pBindings[i] != create_info.pBindings[i]) {
                bindings_same = false;
                break;
            }
        }

        if (!bindings_same) {
            continue;
        }

        result = existing_descriptor_set_layout;
    }
    return result;
}

VkPipelineLayout MaterialTable::find_pipeline_layout(
                                      const VkPipelineLayoutCreateInfo& create_info) {
    VkPipelineLayout result = VK_NULL_HANDLE;
    for (const auto& pipeline_layout : pipeline_layouts) {
        const VkPipelineLayoutCreateInfo& existing_create_info = pipeline_layout.first;
        const VkPipelineLayout& existing_pipeline_layout       = pipeline_layout.second;

        if (existing_create_info.setLayoutCount != create_info.setLayoutCount) {
            continue;
        }

        if (existing_create_info.pushConstantRangeCount != create_info.pushConstantRangeCount) {
            continue;
        }

        bool set_layouts_same = true;
        for (size_t i = 0; i < existing_create_info.setLayoutCount; i++) {
            const VkDescriptorSetLayout& existing_set_layout = existing_create_info.pSetLayouts[i];
            const VkDescriptorSetLayout& set_layout          = create_info.pSetLayouts[i];
            if (existing_set_layout != set_layout) {
                set_layouts_same = false;
                break;
            }
        }

        if (!set_layouts_same) {
            continue;
        }

        bool push_constant_ranges_same = true;
        for (size_t i = 0; i < create_info.pushConstantRangeCount; i++) {
            const VkPushConstantRange& push_constant_range
                = existing_create_info.pPushConstantRanges[i];

            // Attempt to find an equivalent push constant range
            bool range_found = false;
            for (size_t j = 0; j < existing_create_info.pushConstantRangeCount; j++) {
                const VkPushConstantRange& existing_push_constant_range
                    = existing_create_info.pPushConstantRanges[j];
                if (existing_push_constant_range.offset == push_constant_range.offset
                    || existing_push_constant_range.size == push_constant_range.size
                    || existing_push_constant_range.stageFlags == push_constant_range.stageFlags) {
                    range_found = true;
                    break;
                }
            }
            if (!range_found) {
                push_constant_ranges_same = false;
                break;
            }
        }

        if (!push_constant_ranges_same) {
            continue;
        }

        result = existing_pipeline_layout;
        break;
    }
    return result;
}

VkShaderModule MaterialTable::request_shader_module(const VkShaderModuleCreateInfo& create_info) {
    VkShaderModule result = find_shader_module(create_info);
    if (result) {
        return result;
    }

    // Create new module
    VkShaderModuleCreateInfo create_info_copy = create_info;
    if (create_info.codeSize > 0) {
        create_info_copy.pCode = new uint32_t[create_info.codeSize / 4];
        memcpy((void*) create_info_copy.pCode, (void*) create_info.pCode, create_info.codeSize);
    }

    vkCreateShaderModule(app.device, &create_info, nullptr, &result);
    shader_modules.push_back(
        std::pair< VkShaderModuleCreateInfo, VkShaderModule >(create_info_copy, result));

    return result;
}

VkDescriptorSetLayout
MaterialTable::request_descriptor_set_layout(const VkDescriptorSetLayoutCreateInfo& create_info) {
    VkDescriptorSetLayout result = find_descriptor_set_layout(create_info);
    if (result) {
        return result;
    }

    // Create new layout
    VkDescriptorSetLayoutCreateInfo create_info_copy = create_info;
    if (create_info.bindingCount > 0) {
        create_info_copy.pBindings = new VkDescriptorSetLayoutBinding[create_info.bindingCount];
        memcpy((void*) create_info_copy.pBindings, (void*) create_info.pBindings,
               create_info.bindingCount * sizeof(VkDescriptorSetLayoutBinding));
    }

    vkCreateDescriptorSetLayout(this->app.device, &create_info, nullptr, &result);
    this->descriptor_set_layouts.push_back(
        std::pair< VkDescriptorSetLayoutCreateInfo, VkDescriptorSetLayout >(create_info_copy,
                                                                            result));

    return result;
}

VkPipelineLayout MaterialTable::request_pipeline_layout(const VkPipelineLayoutCreateInfo& create_info) {
    VkPipelineLayout result = find_pipeline_layout(create_info);
    if (result) {
        return result;
    }

    // Create new layout
    VkPipelineLayoutCreateInfo create_info_copy = create_info;
    if (create_info.pushConstantRangeCount > 0) {
        create_info_copy.pPushConstantRanges
            = new VkPushConstantRange[create_info.pushConstantRangeCount];
        memcpy((void*) create_info_copy.pPushConstantRanges,
               (void*) create_info.pPushConstantRanges,
               create_info.pushConstantRangeCount * sizeof(VkPushConstantRange));
    }

    if (create_info.setLayoutCount > 0) {
        create_info_copy.pSetLayouts = new VkDescriptorSetLayout[create_info.setLayoutCount];
        memcpy((void*) create_info_copy.pSetLayouts, (void*) create_info.pSetLayouts,
               create_info.setLayoutCount * sizeof(VkDescriptorSetLayout));
    }
    vkCreatePipelineLayout(app.device, &create_info, nullptr, &result);
    pipeline_layouts.push_back(
        std::pair< VkPipelineLayoutCreateInfo, VkPipelineLayout >(create_info_copy, result));

    return result;
}

VkPipeline MaterialTable::request_pipeline(const VkGraphicsPipelineCreateInfo& create_info) {
    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(this->app.device, this->pipeline_cache, 1,
                                       &create_info, nullptr, &pipeline));
    return pipeline;
}

VkPipelineLayout
MaterialTable::create_pipeline_layout_from_modules(std::vector< ShaderModuleData >& shader_module_data) {
    VkShaderStageFlags stages_seen = 0;

    // Create pipeline layout
    for (const auto& module : shader_module_data) {
        // Parse reflection data
        // Check if stage overlaps with something we've seen already
        ASSERT_MSG(!(stages_seen & module.stage), "Duplicate shader stage detected");
        stages_seen |= module.stage;
    }

    // Check if any descriptor layouts can be reused
    // Flatten list of descriptor set bindings
    std::vector< ShaderModuleData::DescriptorSetBinding > descriptor_set_bindings;
    for (const auto& module : shader_module_data) {
        descriptor_set_bindings.insert(descriptor_set_bindings.begin(), module.set_bindings.begin(),
                                       module.set_bindings.end());
    }

    // Get max set index
    auto max_index_iter = std::max_element(
        descriptor_set_bindings.begin(), descriptor_set_bindings.end(),
        [](const auto& left, const auto& right) { return left.set_number < right.set_number; });

    size_t max_set_index
        = max_index_iter != descriptor_set_bindings.end() ? max_index_iter->set_number : 0;

    // Create descriptor set binding lists
    std::vector< std::vector< VkDescriptorSetLayoutBinding > > set_binding_map(max_set_index + 1);
    for (const auto& descriptor_set_binding : descriptor_set_bindings) {
        // Get binding list for a given set index
        std::vector< VkDescriptorSetLayoutBinding >& set_binding_list
            = set_binding_map[descriptor_set_binding.set_number];

        // Insert layout binding sorted to facilitate comparison
        auto sorted_pos = std::upper_bound(
            set_binding_list.begin(), set_binding_list.end(), descriptor_set_binding.binding,
            [](const VkDescriptorSetLayoutBinding& left,
               const VkDescriptorSetLayoutBinding& right) { return left.binding < right.binding; });
        set_binding_list.insert(sorted_pos, descriptor_set_binding.binding);
    }

    // Request descriptor set layouts
    std::vector< VkDescriptorSetLayout > vk_descriptor_set_layouts;
    for (size_t set_index = 0; set_index < set_binding_map.size(); set_index++) {
        const auto& binding_list = set_binding_map[set_index];

        VkDescriptorSetLayoutCreateInfo create_info = {};
        create_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        create_info.pBindings    = binding_list.size() > 0 ? binding_list.data() : nullptr;
        create_info.bindingCount = binding_list.size();

        vk_descriptor_set_layouts.push_back(
            request_descriptor_set_layout(create_info));
    }

    // Check if any pipeline layouts can be reused
    VkPipelineLayoutCreateInfo layout_create_info = {};
    layout_create_info.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_create_info.pSetLayouts                = vk_descriptor_set_layouts.data();
    layout_create_info.setLayoutCount             = vk_descriptor_set_layouts.size();
    // TODO: Push constants

    return request_pipeline_layout(layout_create_info);
}

}    // namespace Vulkan