find_package(glm CONFIG REQUIRED)

cth_glob_cpp(CUBE_SRC "src")

add_executable(cube ${CUBE_SRC})

cth_target_enable_build_cache(cube OPTIONAL)

target_link_libraries(cube PRIVATE cgfw jvk timgui glm::glm-header-only)

target_compile_definitions(cube PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)

# SPIR-V in the build directory of the target, the program reads it from there
set(CUBE_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(CUBE_SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
get_target_property(JVK_GLSL_INCLUDE_DIR jvk JVK_GLSL_INCLUDE_DIR)

set(CUBE_GLSL "${CUBE_SHADER_SRC_DIR}/cube.vert" "${CUBE_SHADER_SRC_DIR}/cube.frag")
# the includes of the shaders: a change compiles them again
set(CUBE_GLSL_INCLUDES "${CUBE_SHADER_SRC_DIR}/cube.glsl" "${JVK_GLSL_INCLUDE_DIR}/jvk/heaps.glsl")
set(CUBE_SPIRV "")

foreach (GLSL_FILE ${CUBE_GLSL})
    get_filename_component(GLSL_NAME ${GLSL_FILE} NAME)
    set(SPIRV_FILE "${CUBE_SHADER_DIR}/${GLSL_NAME}.spv")

    add_custom_command(
            OUTPUT ${SPIRV_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CUBE_SHADER_DIR}
            COMMAND ${GLSLC_EXECUTABLE} --target-env=vulkan1.3 -I ${JVK_GLSL_INCLUDE_DIR} -I ${CUBE_SHADER_SRC_DIR}
            ${GLSL_FILE} -o ${SPIRV_FILE}
            DEPENDS ${GLSL_FILE} ${CUBE_GLSL_INCLUDES}
            COMMENT "Compiling ${GLSL_NAME}"
            VERBATIM
    )

    list(APPEND CUBE_SPIRV ${SPIRV_FILE})
endforeach ()

add_custom_target(cube_shaders DEPENDS ${CUBE_SPIRV} SOURCES ${CUBE_GLSL} ${CUBE_GLSL_INCLUDES})
add_dependencies(cube cube_shaders)

target_compile_definitions(cube PRIVATE CUBE_SHADER_DIR="${CUBE_SHADER_DIR}")
