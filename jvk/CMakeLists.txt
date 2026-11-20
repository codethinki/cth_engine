cth_glob_cpp(JVK_SRC "src")
cth_glob_cpp(JVK_INCL "incl")

add_library(jvk STATIC ${JVK_SRC} ${JVK_INCL})

cth_target_enable_build_cache(jvk OPTIONAL)

# the shader convention of the jvk heaps, the single source of jvk/heap_bindings.hpp and jvk/heaps.glsl
set(JVK_HEAP_SET 0)
set(JVK_HEAP_SAMPLED_IMAGE_BINDING 0)
set(JVK_HEAP_STORAGE_IMAGE_BINDING 1)
set(JVK_HEAP_SAMPLER_BINDING 2)

set(JVK_GENERATED_INCL "${CMAKE_CURRENT_BINARY_DIR}/generated/incl")
configure_file(config/heap_bindings.hpp.in "${JVK_GENERATED_INCL}/jvk/heap_bindings.hpp" @ONLY)
configure_file(config/heaps.glsl.in "${JVK_GENERATED_INCL}/jvk/heaps.glsl" @ONLY)

target_include_directories(
        jvk
        PUBLIC incl "${JVK_GENERATED_INCL}"
        PRIVATE src
)

# glslc -I of the shaders that include "jvk/heaps.glsl": get_target_property(<var> jvk JVK_GLSL_INCLUDE_DIR)
set_target_properties(jvk PROPERTIES JVK_GLSL_INCLUDE_DIR "${JVK_GENERATED_INCL}")

target_precompile_headers(
        jvk
        PRIVATE
        src/pch/pch.hpp
)

target_link_libraries(
        jvk
        PUBLIC cvk cth::coro
)

# tests
if(JOLLY_ENABLE_TESTS)
    add_subdirectory(tests)
endif()
