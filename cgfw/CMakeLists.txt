cth_glob_cpp(CGFW_SRC "src")
cth_glob_cpp(CGFW_INCL "incl")

add_library(cgfw STATIC ${CGFW_SRC} ${CGFW_INCL})

cth_target_enable_build_cache(cgfw OPTIONAL)

target_include_directories(
        cgfw
        PUBLIC incl
        PRIVATE src
)

target_precompile_headers(
        cgfw
        PRIVATE
        src/pch/pch.hpp
)

# the RGFW implementation is C, cth_glob_cpp() skips .c files, the pch is C++
target_sources(cgfw PRIVATE src/window/rgfw.c)
set_source_files_properties(src/window/rgfw.c PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

# the public headers do not include RGFW.h
target_link_libraries(
        cgfw
        PUBLIC cvk
        PRIVATE rgfw::rgfw
)

# tests
if(JOLLY_ENABLE_TESTS)
    add_subdirectory(tests)
endif()
