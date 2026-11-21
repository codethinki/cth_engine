cth_glob_cpp(TRIANGLE_SRC "src")

add_executable(triangle ${TRIANGLE_SRC})

cth_target_enable_build_cache(triangle OPTIONAL)

target_link_libraries(triangle PRIVATE cgfw jvk)

# SPIR-V in the build directory of the target, the program reads it from there
set(TRIANGLE_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(TRIANGLE_GLSL
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag"
)
set(TRIANGLE_SPIRV "")

foreach (GLSL_FILE ${TRIANGLE_GLSL})
    get_filename_component(GLSL_NAME ${GLSL_FILE} NAME)
    set(SPIRV_FILE "${TRIANGLE_SHADER_DIR}/${GLSL_NAME}.spv")

    add_custom_command(
            OUTPUT ${SPIRV_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${TRIANGLE_SHADER_DIR}
            COMMAND ${GLSLC_EXECUTABLE} --target-env=vulkan1.3 ${GLSL_FILE} -o ${SPIRV_FILE}
            DEPENDS ${GLSL_FILE}
            COMMENT "Compiling ${GLSL_NAME}"
            VERBATIM
    )

    list(APPEND TRIANGLE_SPIRV ${SPIRV_FILE})
endforeach ()

add_custom_target(triangle_shaders DEPENDS ${TRIANGLE_SPIRV} SOURCES ${TRIANGLE_GLSL})
add_dependencies(triangle triangle_shaders)

target_compile_definitions(triangle PRIVATE TRIANGLE_SHADER_DIR="${TRIANGLE_SHADER_DIR}")
