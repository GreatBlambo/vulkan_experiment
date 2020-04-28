file(GLOB JSON "resources/*.json")

set(FILES ${JSON})

if (NOT FILES)
    message("No static resources")
    set(USING_STATIC_RESOURCE false PARENT_SCOPE)
    return()
endif()

set(PYTHON_COMMAND python)

set(STATIC_ROOT "${CMAKE_CURRENT_BINARY_DIR}")
set(STATIC_HEADER_DIR "${STATIC_ROOT}/static" )
set(STATIC_SOURCE_DIR "${STATIC_ROOT}" )

set(HEADER_LOC "${STATIC_HEADER_DIR}/static_resources.h")
set(SOURCE_LOC "${STATIC_SOURCE_DIR}/static_resources.cpp")

set(GENERATE_STATIC_SOURCES_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/generate_static_sources.py")

add_custom_command(
    OUTPUT ${HEADER_LOC} ${SOURCE_LOC}
    COMMAND 
        ${PYTHON_COMMAND} 
        ${GENERATE_STATIC_SOURCES_SCRIPT}
        --input_files ${FILES}
        --out-h-dir=${STATIC_HEADER_DIR} 
        --out-c-dir=${STATIC_SOURCE_DIR} 
        --accessor-name="static_resources"
    DEPENDS ${FILES} ${GENERATE_STATIC_SOURCES_SCRIPT}
)

add_custom_target(
    "static_resource_sources"
    ALL
    DEPENDS
        ${HEADER_LOC} ${SOURCE_LOC}
)

set(STATIC_SOURCE_FILES ${HEADER_LOC} ${SOURCE_LOC})
if(STATIC_SOURCE_FILES)
    add_library(static_resources STATIC ${STATIC_SOURCE_FILES})
    target_include_directories(
        static_resources 
        PUBLIC $<BUILD_INTERFACE:${STATIC_ROOT}>
    )
    set(USING_STATIC_RESOURCES true PARENT_SCOPE)
else()
    set(USING_STATIC_RESOURCES false PARENT_SCOPE)
endif()
