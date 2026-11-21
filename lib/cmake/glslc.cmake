# glslc of the Vulkan SDK or of the system: the build compiles the shaders with it
# sets GLSLC_EXECUTABLE, no glslc -> configure error
cth_find_program(GLSLC_EXECUTABLE glslc HINTS ENV VULKAN_SDK PATH_SUFFIXES bin Bin)
message(STATUS "glslc: ${GLSLC_EXECUTABLE}")
