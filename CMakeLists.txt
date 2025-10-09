cmake_minimum_required(VERSION 3.21)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_PREFIX_PATH "${CTH_INSTALL_PATH}")

include(glob_cpp)
include(add_resources)

find_package(cth REQUIRED)

project(jolly LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


enable_testing()


# Add subprojects
add_subdirectory(jvk)
add_subdirectory(jolly)
add_subdirectory(demo)
add_subdirectory(tests)
