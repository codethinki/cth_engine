# cth from the lib/cth submodule
if(NOT TARGET cth::cth)
    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/../cth/CMakeLists.txt")
        message(FATAL_ERROR "lib/cth is empty, run: git submodule update --init lib/cth")
    endif()

    # the engine needs cth and cth_os
    set(CTH_BUILD_CORO OFF)
    set(CTH_BUILD_NET OFF)
    set(CTH_BUILD_WIN OFF)

    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../cth" "${CMAKE_BINARY_DIR}/lib/cth")
endif()
