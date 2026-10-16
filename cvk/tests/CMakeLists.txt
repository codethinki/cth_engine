cth_glob_cpp(CVK_TESTS_SRC "src")

add_executable(cvk_tests ${CVK_TESTS_SRC})

cth_target_enable_build_cache(cvk_tests OPTIONAL)

target_include_directories(
        cvk_tests
        PRIVATE src
)

target_compile_definitions(cvk_tests PRIVATE CVK_TEST_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders")

find_package(GTest CONFIG REQUIRED)

target_link_libraries(cvk_tests PRIVATE GTest::gtest_main cvk)

include(GoogleTest)
gtest_discover_tests(cvk_tests)
