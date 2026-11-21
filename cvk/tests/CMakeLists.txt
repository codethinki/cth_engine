cth_glob_cpp(CVK_TESTS_SRC "src")

add_executable(cvk_tests ${CVK_TESTS_SRC})

cth_target_enable_build_cache(cvk_tests OPTIONAL)

target_include_directories(
        cvk_tests
        PRIVATE src
)

target_precompile_headers(
        cvk_tests
        PRIVATE
        src/pch/pch.hpp
)

# the test shaders: GLSL objects, the tests load them from the object folder
cth_glob(CVK_TESTS_GLSL "shaders" PATTERNS *.vert *.frag *.comp)
add_library(cvk_tests_shaders OBJECT ${CVK_TESTS_GLSL})
add_dependencies(cvk_tests cvk_tests_shaders)

# the shaders of one source folder share one object folder
target_compile_definitions(
        cvk_tests
        PRIVATE
        CVK_TEST_SHADER_DIR="$<PATH:GET_PARENT_PATH,$<LIST:GET,$<TARGET_OBJECTS:cvk_tests_shaders>,0>>"
)

find_package(GTest CONFIG REQUIRED)

target_link_libraries(cvk_tests PRIVATE GTest::gtest_main cvk)

# one process: the tests share their instance and device
add_test(NAME cvk_tests COMMAND cvk_tests)
