add_subdirectory(demo_shaders)

cth_glob_cpp(DEMO_SRC "src")
add_executable(demo ${DEMO_SRC})

cth_add_resources(demo "res")
add_dependencies(demo demo_shaders)


target_include_directories(
        demo PUBLIC
        incl
)

target_precompile_headers(
        demo PRIVATE
        src/pch/pch.hpp
)

find_package(cth CONFIG REQUIRED)

target_link_libraries(demo PRIVATE jvk jolly cth::cth)
