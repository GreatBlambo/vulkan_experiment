#version 450
#extension GL_ARB_separate_shader_objects : enable

struct MVP {
    mat4 model;
    mat4 view;
    mat4 proj;
};

layout(set = 4, binding = 0) uniform UniformBufferObject {
    MVP mvp;
} ubo;

layout(set = 5, binding = 0) uniform UniformBufferObject2 {
    MVP mvp;
} ubo2;

layout(push_constant) uniform PushConstant {
    mat4 push_mat4;
};

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(texSampler, fragTexCoord);
}