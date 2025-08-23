project(demo LANGUAGES CXX)

add_subdirectory(demo_shaders)

glob_cpp(DEMO_SRC "src")
add_executable(demo ${DEMO_SRC})

add_resources(demo "res")
add_dependencies(demo demo_shaders)


target_include_directories(
        demo PUBLIC
        incl
        ../../cth/
)

target_precompile_headers(
        demo PRIVATE
        src/pch/pch.hpp
)

target_link_libraries(demo PRIVATE jvk jolly)

