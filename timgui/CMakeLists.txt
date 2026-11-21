find_package(imgui CONFIG REQUIRED)
find_package(implot CONFIG REQUIRED)

cth_glob_cpp(TIMGUI_SRC "src")
cth_glob_cpp(TIMGUI_INCL "incl")

add_library(timgui STATIC ${TIMGUI_SRC} ${TIMGUI_INCL})

cth_target_enable_build_cache(timgui OPTIONAL)

# the SPIR-V of the shaders in generated headers, the library embeds it: no SPIR-V files at run time
set(TIMGUI_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(TIMGUI_SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(TIMGUI_GENERATED_SRC "${CMAKE_CURRENT_BINARY_DIR}/generated/src")
set(TIMGUI_SPIRV_HEADER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/spirv_header.cmake")
get_target_property(JVK_GLSL_INCLUDE_DIR jvk JVK_GLSL_INCLUDE_DIR)

set(TIMGUI_GLSL "${TIMGUI_SHADER_SRC_DIR}/imgui.vert" "${TIMGUI_SHADER_SRC_DIR}/imgui.frag")
set(TIMGUI_GLSL_SHARED "${TIMGUI_SHADER_SRC_DIR}/imgui.glsl")
# the includes of the shaders: a change compiles them again
set(TIMGUI_GLSL_INCLUDES ${TIMGUI_GLSL_SHARED} "${JVK_GLSL_INCLUDE_DIR}/jvk/heaps.glsl")
set(TIMGUI_SHADER_HEADERS "")

foreach (GLSL_FILE ${TIMGUI_GLSL})
    get_filename_component(GLSL_NAME ${GLSL_FILE} NAME)
    # imgui.vert -> shaders/imgui_vert.hpp with IMGUI_VERT
    string(REPLACE "." "_" HEADER_NAME ${GLSL_NAME})
    string(TOUPPER ${HEADER_NAME} ARRAY_NAME)
    set(SPIRV_FILE "${TIMGUI_SHADER_DIR}/${GLSL_NAME}.spv")
    set(HEADER_FILE "${TIMGUI_GENERATED_SRC}/shaders/${HEADER_NAME}.hpp")

    add_custom_command(
            OUTPUT ${HEADER_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${TIMGUI_SHADER_DIR}
            COMMAND ${GLSLC_EXECUTABLE} --target-env=vulkan1.3 -O
            -I ${JVK_GLSL_INCLUDE_DIR} -I ${TIMGUI_SHADER_SRC_DIR} ${GLSL_FILE} -o ${SPIRV_FILE}
            COMMAND ${CMAKE_COMMAND}
            -DSPIRV_FILE=${SPIRV_FILE} -DHEADER_FILE=${HEADER_FILE} -DARRAY_NAME=${ARRAY_NAME}
            -P ${TIMGUI_SPIRV_HEADER_SCRIPT}
            DEPENDS ${GLSL_FILE} ${TIMGUI_GLSL_INCLUDES} ${TIMGUI_SPIRV_HEADER_SCRIPT}
            COMMENT "Compiling ${GLSL_NAME}"
            VERBATIM
    )

    list(APPEND TIMGUI_SHADER_HEADERS ${HEADER_FILE})
endforeach ()

# the generated headers as sources: the build writes them before it compiles the library
target_sources(timgui PRIVATE ${TIMGUI_SHADER_HEADERS} ${TIMGUI_GLSL} ${TIMGUI_GLSL_SHARED})

target_include_directories(
        timgui
        PUBLIC incl
        PRIVATE src "${TIMGUI_GENERATED_SRC}"
)

target_precompile_headers(
        timgui
        PRIVATE
        src/pch/pch.hpp
)

# the public header declares cgfw::Window only
# implot before imgui: the implot target does not link imgui, a static link needs the order
target_link_libraries(
        timgui
        PUBLIC jvk implot::implot imgui::imgui
        PRIVATE cgfw
)

# tests
if (JOLLY_ENABLE_TESTS)
    add_subdirectory(tests)
endif ()
