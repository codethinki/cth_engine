project(run_tests LANGUAGES CXX)

glob_cpp(TEST_SRC "src")

add_executable(run_tests ${TEST_SRC})

target_include_directories(
    run_tests PRIVATE
        incl
        ../../cth/
)

find_package(GTest CONFIG REQUIRED)

target_link_libraries(run_tests PRIVATE GTest::gtest_main jvk jolly)

include(GoogleTest)
gtest_discover_tests(run_tests)