cth_glob_cpp(JVK_SRC "src")
cth_glob_cpp(JVK_INCL "incl")

add_library(jvk STATIC ${JVK_SRC} ${JVK_INCL})

target_include_directories(
        jvk
        PUBLIC
        incl
)

target_precompile_headers(
        jvk
        PRIVATE
        src/pch/pch.hpp
)

find_package(volk CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS pfr)
find_package(cth CONFIG REQUIRED COMPONENTS cth)

target_link_libraries(jvk PUBLIC volk::volk glm::glm Boost::pfr cth::cth cth::win)