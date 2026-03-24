cmake_minimum_required(VERSION 4.0.0)

project(jolly LANGUAGES CXX)

include(cth_target_utilities)
include(cth_setup_utilities)

cth_set_compiler_specifics()
cth_set_latest_c_cpp_standard()

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
 
# Add subprojects
add_subdirectory(jvk)
add_subdirectory(jolly)
add_subdirectory(demo)


# Tests
enable_testing() 

add_subdirectory(tests)
