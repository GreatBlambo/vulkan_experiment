#include "vulkan_app.h"
#include "vulkan_utils.h"

#include "utils.h"

#include "renderer.h"

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

void Renderer::deserialize_reflection_data(const Memory::Buffer& reflection_json,
                                           ShaderModuleCreateInfo& out_reflection_data) {
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
                input_attrib.name   = allocator.copy_string(input["name"].GetString());
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

    auto& string_allocator                 = this->string_allocator;
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

Renderer::Renderer(Vulkan::App& app, Memory::IAllocator& allocator)
    : app(app)
    , allocator(allocator)
    , string_allocator(KB(10), allocator) {
    VkPipelineCacheCreateInfo pipeline_cache_create_info = {};
    pipeline_cache_create_info.sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    pipeline_cache_create_info.initialDataSize = 0;
    VK_CHECK(vkCreatePipelineCache(app.device, &pipeline_cache_create_info, nullptr,
                                   &this->pipeline_cache));
}

Renderer::~Renderer() {
    for (const auto& shader_module : shader_modules) {
        vkDestroyShaderModule(app.device, shader_module.second, nullptr);
    }

    for (const auto& descriptor_set_layout : descriptor_set_layout_cache) {
        vkDestroyDescriptorSetLayout(app.device, descriptor_set_layout.second, nullptr);
    }

    for (const auto& pipeline_layout : pipeline_layout_cache) {
        vkDestroyPipelineLayout(app.device, pipeline_layout.second, nullptr);
    }

    for (const auto& pipeline : pipelines) {
        vkDestroyPipeline(app.device, pipeline, nullptr);
    }

    vkDestroyPipelineCache(app.device, pipeline_cache, nullptr);
    string_allocator.release();
}

ShaderModule Renderer::create_shader_module(const char* name, const Memory::Buffer& spirv_source,
                                            const ShaderModuleCreateInfo& create_info) {
    VkShaderModuleCreateInfo vk_create_info = {};
    vk_create_info.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vk_create_info.codeSize                 = spirv_source.size;
    vk_create_info.pCode                    = (uint32_t*) spirv_source.size;

    VkShaderModule vk_module;
    VK_CHECK(vkCreateShaderModule(app.device, &vk_create_info, nullptr, &vk_module));

    shader_modules.push_back(
        std::pair< ShaderModuleCreateInfo, VkShaderModule >(create_info, vk_module));

    ShaderModule id = { shader_modules.size() - 1 };

    name_to_shader_module[name] = id;
    return id;
}

ShaderModule Renderer::create_shader_module(const char* name, const Memory::Buffer& spirv_source,
                                            const Memory::Buffer& reflection_json) {
    ShaderModuleCreateInfo new_shader_module;
    deserialize_reflection_data(reflection_json, new_shader_module);

    return create_shader_module(name, spirv_source, new_shader_module);
}

VkPipeline Renderer::request_pipeline(const VkGraphicsPipelineCreateInfo& create_info) {
    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(app.device, this->pipeline_cache, 1, &create_info, nullptr,
                                       &pipeline));
    pipelines.push_back(pipeline);
    return pipeline;
}

VkDescriptorSetLayout
Renderer::request_descriptor_set_layout(const DescriptorSetLayoutCreateInfo& create_info) {
    auto it = descriptor_set_layout_cache.find(create_info);
    if (it != descriptor_set_layout_cache.end()) {
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
    VK_CHECK(vkCreateDescriptorSetLayout(app.device, &vk_create_info, nullptr, &set_layout));

    descriptor_set_layout_cache[create_info] = set_layout;
    return set_layout;
}

VkPipelineLayout Renderer::request_pipeline_layout(const PipelineLayoutCreateInfo& create_info) {
    auto it = pipeline_layout_cache.find(create_info);
    if (it != pipeline_layout_cache.end()) {
        return it->second;
    }

    VkDescriptorSetLayout set_layouts[VULKAN_MAX_DESCRIPTOR_SETS];

    VkPipelineLayoutCreateInfo vk_create_info = {};
    vk_create_info.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    vk_create_info.pPushConstantRanges        = &create_info.push_constant_range;
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
    VK_CHECK(vkCreatePipelineLayout(app.device, &vk_create_info, nullptr, &pipeline_layout));

    pipeline_layout_cache[create_info] = pipeline_layout;
    return pipeline_layout;
}

void render_frame(Vulkan::App& app, const std::function< void(const size_t, VkCommandBuffer) >& render) {
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

}    // namespace Vulkan