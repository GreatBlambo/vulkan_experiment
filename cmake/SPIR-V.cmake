find_program(GLSLANGVALIDATOR
	glslangValidator)
if(NOT GLSLANGVALIDATOR)
	message(FATAL_ERROR "glslangValidator was not found. Install the Vulkan SDK.")
endif()

find_program(SPIRVCROSS
	spirv-cross)
if(NOT SPIRVCROSS)
	message(FATAL_ERROR "spirv-cross was not found. Install the Vulkan SDK.")
endif()

function(spirv_shader)
    cmake_parse_arguments(
        SPIRV_SHADER
        "" 
        "OUTPUT_DEST" 
        "SRCS"
        ${ARGN}
    )

    if(NOT SPIRV_SHADER_OUTPUT_DEST)
        message("Please set OUTPUT_DEST")
        return()
    endif()

    foreach(GLSL ${SPIRV_SHADER_SRCS})
        get_filename_component(GLSL_FILENAME ${GLSL} NAME)
        string(CONCAT OUT_FILE_PATH ${SPIRV_SHADER_OUTPUT_DEST} "/" ${GLSL_FILENAME} ".spv")
        string(CONCAT JSON_FILE_PATH ${SPIRV_SHADER_OUTPUT_DEST} "/" ${GLSL_FILENAME} ".json")
        add_custom_command(
            OUTPUT ${OUT_FILE_PATH}
            COMMAND ${GLSLANGVALIDATOR} -V ${GLSL} -o ${OUT_FILE_PATH}
            DEPENDS ${GLSL})
        add_custom_command(
            OUTPUT ${JSON_FILE_PATH}
            COMMAND ${SPIRVCROSS} --output ${JSON_FILE_PATH} ${OUT_FILE_PATH} --reflect
            DEPENDS ${OUT_FILE_PATH})
        add_custom_target(
            ${GLSL_FILENAME} ALL DEPENDS ${OUT_FILE_PATH} ${JSON_FILE_PATH}
        )
	endforeach()
endfunction(spirv_shader)
