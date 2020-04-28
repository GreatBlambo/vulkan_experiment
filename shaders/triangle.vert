#version 450
#extension GL_GOOGLE_include_directive : enable

#include "utils.glsl"

vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

layout (location = 0) out vec3 frag_colors;

layout (set = 0, binding = 0) uniform Unfirms {
    SomeStruct test;
};

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    frag_colors = colors[gl_VertexIndex];
}