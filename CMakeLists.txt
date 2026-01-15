cmake_minimum_required(VERSION 3.21)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")


include(setup_utilities)
include(target_utilities)



project(jolly LANGUAGES CXX)
set_compiler_specifics()
set_cxx_standard()


# Add subprojects
add_subdirectory(jvk)
add_subdirectory(jolly)
add_subdirectory(demo)


# Tests
enable_testing() 

add_subdirectory(tests)
