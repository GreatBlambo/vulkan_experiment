include(SPIR-V)

file(GLOB SHADER_SOURCES "*.vert" "*.frag" "*.geom" "*.tesc" "*.tese" "*.comp")

spirv_shader(
    OUTPUT_DEST "${PROJECT_SOURCE_DIR}/app/shaders"
    SRCS ${SHADER_SOURCES}
)
