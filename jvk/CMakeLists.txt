cth_glob_cpp(JVK_SRC "src")
cth_glob_cpp(JVK_INCL "incl")

add_library(jvk STATIC ${JVK_SRC} ${JVK_INCL})

cth_target_enable_build_cache(jvk OPTIONAL)

target_include_directories(
        jvk
        PUBLIC incl
        PRIVATE src
)

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
