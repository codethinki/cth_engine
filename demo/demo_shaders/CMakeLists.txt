function(glob_glsl VAR SUB_PATH)
    cth_glob(
        GLSL_FILES
        ${SUB_PATH}
        PATTERNS
            "*.frag"
            "*.vert"
    )
    set(${VAR} ${GLSL_FILES} PARENT_SCOPE)
    message("glsl files: ${GLSL_FILES}")
endfunction()

cth_find_program(
    GLSLC_EXECUTABLE    
    glslc 
    HINTS ENV VULKAN_SDK 
    PATH_SUFFIXES bin 
    DOC "Path to the GLSL SPRIR-V compiler"
)

message(STATUS "Found glslc compiler: ${GLSLC_EXECUTABLE}")

glob_glsl(SHADER_FILES "src")

set(SPIRV_OUTPUTS "")

foreach(SHADER_FILE ${SHADER_FILES})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)
    get_filename_component(SHADER_EXT ${SHADER_FILE} EXT)

    # Create unique output name to avoid conflicts
    set(OUTPUT_SPIRV "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}${SHADER_EXT}.spv")

    # This command tells CMake how to compile a single shader file.
    add_custom_command(
        OUTPUT ${OUTPUT_SPIRV}
        COMMAND ${GLSLC_EXECUTABLE} ${SHADER_FILE} -o ${OUTPUT_SPIRV}
        DEPENDS ${SHADER_FILE}
        COMMENT "Compiling ${SHADER_FILE}"
    )

    list(APPEND SPIRV_OUTPUTS ${OUTPUT_SPIRV})
endforeach()

add_custom_target(
    demo_shaders ALL
    DEPENDS ${SPIRV_OUTPUTS}
)



source_group(asdf FILES ${SHADER_FILES})