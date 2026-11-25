find_package(imgui CONFIG REQUIRED)
find_package(implot CONFIG REQUIRED)

cth_glob_cpp(TIMGUI_SRC "src")
cth_glob_cpp(TIMGUI_INCL "incl")

add_library(timgui STATIC ${TIMGUI_SRC} ${TIMGUI_INCL})

cth_target_enable_build_cache(timgui OPTIONAL)

# the shaders: GLSL objects, imgui.glsl is an include of them and no object
cth_glob(TIMGUI_GLSL "shaders" PATTERNS *.vert *.frag *.glsl)
add_library(timgui_shaders OBJECT ${TIMGUI_GLSL})
# <jvk/heaps.glsl>
target_link_libraries(timgui_shaders PRIVATE jvk)

#TEMP until MSVC has #embed: generated headers embed the SPIR-V
# the library has the SPIR-V: no SPIR-V files at run time
set(TIMGUI_GENERATED_SRC "${CMAKE_CURRENT_BINARY_DIR}/generated/src")
set(TIMGUI_SPIRV_HEADER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/spirv_header.cmake")
set(TIMGUI_SHADER_HEADERS "")

foreach (GLSL_FILE ${TIMGUI_GLSL})
    get_filename_component(GLSL_NAME ${GLSL_FILE} NAME)
    get_filename_component(GLSL_STAGE ${GLSL_FILE} LAST_EXT)
    string(SUBSTRING ${GLSL_STAGE} 1 -1 GLSL_STAGE)
    # the includes have no object
    if (NOT GLSL_STAGE IN_LIST CMAKE_GLSL_SOURCE_FILE_EXTENSIONS)
        continue()
    endif ()

    # imgui.vert -> object imgui.vert.spv -> shaders/imgui_vert.hpp with IMGUI_VERT
    string(REPLACE "." "\\." SPIRV_NAME_REGEX "/${GLSL_NAME}.spv")
    set(SPIRV_FILE "$<LIST:FILTER,$<TARGET_OBJECTS:timgui_shaders>,INCLUDE,${SPIRV_NAME_REGEX}>")
    string(REPLACE "." "_" HEADER_NAME ${GLSL_NAME})
    string(TOUPPER ${HEADER_NAME} ARRAY_NAME)
    set(HEADER_FILE "${TIMGUI_GENERATED_SRC}/shaders/${HEADER_NAME}.hpp")

    add_custom_command(
            OUTPUT ${HEADER_FILE}
            COMMAND ${CMAKE_COMMAND}
            -DSPIRV_FILE=${SPIRV_FILE} -DHEADER_FILE=${HEADER_FILE} -DARRAY_NAME=${ARRAY_NAME}
            -P ${TIMGUI_SPIRV_HEADER_SCRIPT}
            DEPENDS ${SPIRV_FILE} ${TIMGUI_SPIRV_HEADER_SCRIPT}
            COMMENT "Embedding ${GLSL_NAME}"
            VERBATIM
    )

    list(APPEND TIMGUI_SHADER_HEADERS ${HEADER_FILE})
endforeach ()

# the generated headers as sources: the build writes them before it compiles the library
target_sources(timgui PRIVATE ${TIMGUI_SHADER_HEADERS})
add_dependencies(timgui timgui_shaders)

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
