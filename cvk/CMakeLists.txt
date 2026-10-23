cth_glob_cpp(CVK_SRC "src")
cth_glob_cpp(CVK_INCL "incl")

add_library(cvk STATIC ${CVK_SRC} ${CVK_INCL})

cth_target_enable_build_cache(cvk OPTIONAL)

target_include_directories(
        cvk
        PUBLIC incl
        PRIVATE src
)

target_precompile_headers(
        cvk
        PRIVATE
        src/pch/pch.hpp
)

find_package(volk CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(VulkanUtilityLibraries CONFIG REQUIRED)

target_link_libraries(
        cvk
        PUBLIC volk::volk Vulkan::UtilityHeaders GPUOpen::VulkanMemoryAllocator cth::cth cth::os
)

# tests
if(JOLLY_ENABLE_TESTS)
    add_subdirectory(tests)
endif()
