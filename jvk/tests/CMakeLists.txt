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

# the test shaders: GLSL objects, the tests load them from the object folder
cth_glob(JVK_TESTS_GLSL "shaders" PATTERNS *.comp)
add_library(jvk_tests_shaders OBJECT ${JVK_TESTS_GLSL})
# "jvk/heaps.glsl"
target_link_libraries(jvk_tests_shaders PRIVATE jvk)
add_dependencies(jvk_tests jvk_tests_shaders)

# the shaders of one source folder share one object folder
target_compile_definitions(
        jvk_tests
        PRIVATE
        JVK_TEST_SHADER_DIR="$<PATH:GET_PARENT_PATH,$<LIST:GET,$<TARGET_OBJECTS:jvk_tests_shaders>,0>>"
)

find_package(GTest CONFIG REQUIRED)

target_link_libraries(jvk_tests PRIVATE GTest::gtest_main jvk)

# one process: the tests share their instance and device
add_test(NAME jvk_tests COMMAND jvk_tests)
