# gtl installs only its headers and no cmake package, this creates the target gtl::gtl
if(NOT TARGET gtl::gtl)
    find_path(GTL_INCLUDE_DIR gtl/phmap.hpp REQUIRED)

    add_library(gtl::gtl INTERFACE IMPORTED)
    target_include_directories(gtl::gtl SYSTEM INTERFACE ${GTL_INCLUDE_DIR})
endif()
