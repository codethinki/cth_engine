cth_glob_cpp(TIMGUI_TESTS_SRC "src")

add_executable(timgui_tests ${TIMGUI_TESTS_SRC})

cth_target_enable_build_cache(timgui_tests OPTIONAL)

# ../src: the tests reach the input mapping of timgui
target_include_directories(
        timgui_tests
        PRIVATE src "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)

target_precompile_headers(
        timgui_tests
        PRIVATE
        src/pch/pch.hpp
)

find_package(GTest CONFIG REQUIRED)

target_link_libraries(timgui_tests PRIVATE GTest::gtest_main timgui cgfw)

# one process: the tests share the vulkan loader, RGFW keeps one global state
add_test(NAME timgui_tests COMMAND timgui_tests)
