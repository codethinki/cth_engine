cth_glob_cpp(CGFW_TESTS_SRC "src")

add_executable(cgfw_tests ${CGFW_TESTS_SRC})

cth_target_enable_build_cache(cgfw_tests OPTIONAL)

target_include_directories(
        cgfw_tests
        PRIVATE src
)

target_precompile_headers(
        cgfw_tests
        PRIVATE
        src/pch/pch.hpp
)

find_package(GTest CONFIG REQUIRED)

target_link_libraries(cgfw_tests PRIVATE GTest::gtest_main cgfw)

# one process: RGFW keeps one global state
add_test(NAME cgfw_tests COMMAND cgfw_tests)
