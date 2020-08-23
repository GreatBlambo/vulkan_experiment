cmake_minimum_required(VERSION 3.10.0)

# VCPKG config
message("VCPKG_ROOT $ENV{VCPKG_ROOT}")
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    message(" [INFO] VCPKG CMAKE_TOOLCHAIN_FILE = ${CMAKE_TOOLCHAIN_FILE}")
endif()

# Project config
set(PROJECT_NAME vulkan_experiment)

project(${PROJECT_NAME} VERSION 0.1.0)

include(CTest)
enable_testing()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

find_package(Vulkan REQUIRED)
find_package(assimp CONFIG REQUIRED)
add_subdirectory(thirdparty)
add_subdirectory(static)
add_subdirectory(shaders)

set(CMAKE_EXPORT_COMPILE_COMMANDS true)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/app)

file(GLOB CPP_SOURCES "src/*.cpp" "src/*.h" "src/demos/*.cpp" "src/demos/*.h")
if (0)
    set(CPP_SOURCES
        "src/file_system.cpp"
        "src/file_system.h"
        "src/handle.h"
        "src/hash.h"
        "src/main.cpp"
        "src/memory.cpp"
        "src/platform.cpp"
        "src/platform.h"
        "src/renderer.cpp"
        "src/renderer.h"
        "src/utils.cpp"
        "src/utils.h"
        "src/vulkan_app.cpp"
        "src/vulkan_app.h"
        "src/vulkan_types.h"
        "src/vulkan_utils.cpp"
        "src/vulkan_utils.h"
    )
endif()

add_executable(${PROJECT_NAME} ${CPP_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${PHYSFS_INCLUDE_DIR} ${RAPIDJSON_INCLUDE_DIR} ${PHMAP_INCLUDE_DIR})
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:DEBUG>:APP_DEBUG> GLFW_INCLUDE_NONE GLFW_INCLUDE_VULKAN)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif()

set(LIBS glfw Vulkan::Vulkan physfs-static glm::glm assimp::assimp)
if (${USING_STATIC_RESOURCES})
    list(APPEND LIBS static_resources)
endif()
target_link_libraries(${PROJECT_NAME} ${LIBS})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
