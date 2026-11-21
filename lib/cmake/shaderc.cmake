# glslc of the vcpkg port shaderc, a host dependency in vcpkg.json: the build compiles the shaders with it
# sets GLSLC_EXECUTABLE, no glslc -> configure error

# vcpkg defines VCPKG_HOST_TRIPLET only if the user sets it, a native build has the target triplet as host
if (DEFINED VCPKG_HOST_TRIPLET AND NOT VCPKG_HOST_TRIPLET STREQUAL "")
    set(SHADERC_HOST_TRIPLET "${VCPKG_HOST_TRIPLET}")
else ()
    set(SHADERC_HOST_TRIPLET "${VCPKG_TARGET_TRIPLET}")
endif ()

# the tools of vcpkg first, then the PATH
cth_find_program(
        GLSLC_EXECUTABLE glslc
        HINTS "${VCPKG_INSTALLED_DIR}/${SHADERC_HOST_TRIPLET}/tools/shaderc"
)
message(STATUS "glslc: ${GLSLC_EXECUTABLE}")
