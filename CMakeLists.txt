cmake_minimum_required(VERSION 4.0.0)

project(jolly LANGUAGES CXX)

include(cth_target_utilities)
include(cth_setup_utilities)

cth_set_compiler_specifics()
cth_set_latest_c_cpp_standard()

# the shaders: the GLSL language of cth_cmake, glslc of the Vulkan SDK or of the system
enable_language(GLSL)
# the Vulkan of the engine is a requirement of the project, no user flag of CMAKE_GLSL_FLAGS
add_compile_options($<$<COMPILE_LANGUAGE:GLSL>:--target-env=vulkan1.3>)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# lld links much faster than the default linker
find_program(LLD_LINKER ld.lld)
if (LLD_LINKER AND NOT MSVC)
    set(CMAKE_LINKER_TYPE LLD)
endif ()

include(lib/cmake/cth.cmake)
include(lib/cmake/rgfw.cmake)

if (JOLLY_ENABLE_TESTS)
    enable_testing()
endif ()

# Add subprojects
add_subdirectory(cvk)
add_subdirectory(cgfw)
add_subdirectory(jvk)
add_subdirectory(timgui)

# Demos
add_subdirectory(demo/triangle)
add_subdirectory(demo/cube)

# Tests
if (JOLLY_ENABLE_TESTS)
    add_subdirectory(tests)
endif ()
