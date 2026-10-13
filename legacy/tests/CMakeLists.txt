cth_glob_cpp(TEST_SRC "src")

add_executable(tests ${TEST_SRC})

target_include_directories(
        tests PRIVATE
        incl
        ../../cth/
)

find_package(GTest CONFIG REQUIRED)

target_link_libraries(tests PRIVATE GTest::gtest_main jvk jolly)

include(GoogleTest)
gtest_discover_tests(tests)