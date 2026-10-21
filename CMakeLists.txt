cmake_minimum_required(VERSION 4.0.0)

project(jolly LANGUAGES CXX)

include(cth_target_utilities)
include(cth_setup_utilities)

cth_set_compiler_specifics()
cth_set_latest_c_cpp_standard()

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# lld links much faster than the default linker
find_program(LLD_LINKER ld.lld)
if(LLD_LINKER AND NOT MSVC)
    set(CMAKE_LINKER_TYPE LLD)
endif()

include(lib/cmake/cth.cmake)
include(lib/cmake/gtl.cmake)

if(JOLLY_ENABLE_TESTS)
    enable_testing()
endif()

# Add subprojects
add_subdirectory(cvk)

# Tests
if(JOLLY_ENABLE_TESTS)
    add_subdirectory(tests)
endif()
