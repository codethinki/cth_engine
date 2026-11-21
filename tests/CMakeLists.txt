cth_glob_cpp(TEST_SRC "src")

add_executable(tests ${TEST_SRC})

find_package(GTest CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)

target_link_libraries(tests PRIVATE GTest::gtest_main volk::volk cth::cth)

include(GoogleTest)
gtest_discover_tests(tests)
