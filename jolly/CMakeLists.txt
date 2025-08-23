project(jolly LANGUAGES CXX)

glob_cpp(JOLLY_SRC "src")
glob_cpp(JOLLY_INCL "incl")

add_library(jolly STATIC ${JOLLY_SRC} ${JOLLY_INCL})

target_include_directories(
        jolly PUBLIC
        incl
        ../../cth/
)
target_precompile_headers(
        jolly
        PRIVATE
        src/pch/pch.hpp
)

find_package(glfw3 CONFIG REQUIRED)


target_link_libraries(jolly PUBLIC jvk glfw)

