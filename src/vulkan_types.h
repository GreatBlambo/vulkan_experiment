#pragma once

#include <vulkan/vulkan.h>
#include <parallel_hashmap/phmap.h>

#include "utils.h"
#include "hash.h"
#include "memory.h"

#define VULKAN_MAX_DESCRIPTOR_SETS 8
#define VULKAN_MAX_DESCRIPTOR_BINDINGS 16
#define VULKAN_MAX_VERTEX_INPUTS 8
#define VULKAN_MAX_PUSH_CONSTANT_RANGES 1

namespace Hash {
template <>
struct Hash< VkShaderModuleCreateInfo > {
    inline size_t operator()(const VkShaderModuleCreateInfo& create_info) const {
        return phmap::HashState().combine(
            0, create_info.flags, djb2_hash((char*) create_info.pCode, create_info.codeSize));
    }
};
}    // namespace Hash

namespace Vulkan {

////////////////////////////////////////////////////////////////////////////////
// Hashes
////////////////////////////////////////////////////////////////////////////////

template < typename T >
struct Equals : std::false_type {};

template <>
struct Equals< VkShaderModuleCreateInfo > {
    inline size_t operator()(const VkShaderModuleCreateInfo& l,
                             const VkShaderModuleCreateInfo& r) const {
        using namespace std;
        return memcmp(l.pCode, r.pCode, min(l.codeSize, r.codeSize))
               && l.codeSize == r.codeSize;
    };
};

template <>
struct Equals< VkPushConstantRange > {
    inline size_t operator()(const VkPushConstantRange& l, const VkPushConstantRange& r) const {
        return l.offset == r.offset && l.size == r.size && l.stageFlags == r.stageFlags;
    };
};

////////////////////////////////////////////////////////////////////////////////
// Wrapped types
////////////////////////////////////////////////////////////////////////////////

struct DescriptorBinding {
    const char* name;

    VkDescriptorType descriptor_type;
    uint32_t descriptor_count      = 0;
    VkShaderStageFlags stage_flags = 0;

    inline friend size_t hash_value(const DescriptorBinding& info) {
        return phmap::HashState().combine(0, info.descriptor_type, info.descriptor_count,
                                          info.stage_flags
                                          // TODO: Immutable samplers
        );
    }

    inline bool operator==(const DescriptorBinding& other) const {
        return descriptor_type == other.descriptor_type
               && descriptor_count == other.descriptor_count && stage_flags == other.stage_flags;
    }
};

struct DescriptorSetLayoutCreateInfo {
    DescriptorBinding bindings[VULKAN_MAX_DESCRIPTOR_BINDINGS];

    inline friend size_t hash_value(const DescriptorSetLayoutCreateInfo& info) {
        size_t hash = 0;
        phmap::HashState state;
        for (size_t i = 0; i < VULKAN_MAX_DESCRIPTOR_BINDINGS; i++) {
            if (info.bindings[i].stage_flags == 0) {
                continue;
            }
            hash = state.combine(hash, hash_value(info.bindings[i]));
        }
        return hash;
    }

    inline bool operator==(const DescriptorSetLayoutCreateInfo& other) const {
        for (size_t i = 0; i < VULKAN_MAX_DESCRIPTOR_BINDINGS; i++) {
            if (!(bindings[i] == other.bindings[i])) {
                return false;
            }
        }
        return true;
    }
};

struct PipelineLayoutCreateInfo {
    PipelineLayoutCreateInfo() {
        for (size_t i = 0; i < VULKAN_MAX_DESCRIPTOR_SETS; i++) {
            descriptor_set_layouts[i] = VK_NULL_HANDLE;
        }
    }

    VkDescriptorSetLayout descriptor_set_layouts[VULKAN_MAX_DESCRIPTOR_SETS];
    VkPushConstantRange push_constant_ranges[VULKAN_MAX_PUSH_CONSTANT_RANGES];
    size_t num_push_constant_ranges = 0;

    inline friend size_t hash_value(const PipelineLayoutCreateInfo& info) {
        size_t hash = 0;
        phmap::HashState state;
        for (size_t i = 0; i < VULKAN_MAX_DESCRIPTOR_SETS; i++) {
            if (info.descriptor_set_layouts[i] == VK_NULL_HANDLE) {
                continue;
            }
            hash = state.combine(hash, info.descriptor_set_layouts[i]);
        }
        for (size_t i = 0; i < info.num_push_constant_ranges; i++) {
            hash = state.combine(hash, info.push_constant_ranges[i].offset,
                                 info.push_constant_ranges[i].size,
                                 info.push_constant_ranges[i].stageFlags);
        }
        return hash;
    }

    inline bool operator==(const PipelineLayoutCreateInfo& other) const {
        for (size_t i = 0; i < VULKAN_MAX_DESCRIPTOR_SETS; i++) {
            if (descriptor_set_layouts[i] != other.descriptor_set_layouts[i]) {
                return false;
            }
        }

        if (num_push_constant_ranges != other.num_push_constant_ranges) {
            return false;
        }

        for (size_t i = 0; i < num_push_constant_ranges; i++) {
            if (!Equals< VkPushConstantRange >()(
                push_constant_ranges[i], 
                other.push_constant_ranges[i])) {
                return false;
            }
        }

        return true;
    }
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
    VkShaderStageFlagBits stage;
    ShaderResourceCreateInfo resource_info;
};

struct ShaderSource {
    const char* name;
    const Memory::Buffer spirv_source;
    const Memory::Buffer reflection_json;
};


}    // namespace Vulkan