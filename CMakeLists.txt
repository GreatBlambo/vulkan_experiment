cmake_minimum_required(VERSION 3.10.0)
project(vulkan_experiment VERSION 0.1.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

include(SPIR-V)

include(CTest)
enable_testing()

find_package(Vulkan)
add_subdirectory(thirdparty)

set(CMAKE_EXPORT_COMPILE_COMMANDS true)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/app)

file(GLOB CPP_SOURCES "src/*.cpp")
add_executable(vulkan_experiment ${CPP_SOURCES})

target_include_directories(vulkan_experiment PRIVATE ${Vulkan_INCLUDE_DIRS} ${PHYSFS_INCLUDE_DIR})
target_compile_definitions(vulkan_experiment PRIVATE $<$<CONFIG:DEBUG>:APP_DEBUG>)
target_link_libraries(vulkan_experiment glfw ${Vulkan_LIBRARIES} physfs-static)

set(SHADER_DIR "src/shaders")
file(GLOB SHADER_SOURCES "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag" "${SHADER_DIR}/*.geom" "${SHADER_DIR}/*.tesc" "${SHADER_DIR}/*.tese", "${SHADER_DIR}/*.comp")

spirv_shader(
    OUTPUT_DEST "${PROJECT_SOURCE_DIR}/app/shaders"
    SRCS ${SHADER_SOURCES}
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
