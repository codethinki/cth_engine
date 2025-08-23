project(jvk LANGUAGES CXX)

glob_cpp(JVK_SRC "src")
glob_cpp(JVK_INCL "incl")

add_library(jvk STATIC ${JVK_SRC} ${JVK_INCL})

target_include_directories(
    jvk 
    PUBLIC
        incl
        ../../cth/
)

target_precompile_headers(
        jvk
        PRIVATE
        src/pch/pch.hpp
)

find_package(volk CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS pfr)

target_link_libraries(jvk PUBLIC volk::volk glm::glm Boost::pfr)