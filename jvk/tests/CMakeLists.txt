cth_glob_cpp(JVK_TESTS_SRC "src")

add_executable(jvk_tests ${JVK_TESTS_SRC})

cth_target_enable_build_cache(jvk_tests OPTIONAL)

target_include_directories(
        jvk_tests
        PRIVATE src
)

target_precompile_headers(
        jvk_tests
        PRIVATE
        src/pch/pch.hpp
)

find_package(GTest CONFIG REQUIRED)

target_link_libraries(jvk_tests PRIVATE GTest::gtest_main jvk)

# one process: the tests share their instance and device
add_test(NAME jvk_tests COMMAND jvk_tests)
