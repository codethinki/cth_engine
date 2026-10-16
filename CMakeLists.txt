cmake_minimum_required(VERSION 4.0.0)

project(jolly LANGUAGES CXX)

include(cth_target_utilities)
include(cth_setup_utilities)

cth_set_compiler_specifics()
cth_set_latest_c_cpp_standard()

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

if(JOLLY_ENABLE_TESTS)
    enable_testing()
endif()

# Add subprojects
add_subdirectory(cvk)

# Tests
if(JOLLY_ENABLE_TESTS)
    add_subdirectory(tests)
endif()
