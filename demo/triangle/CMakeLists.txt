cth_glob_cpp(TRIANGLE_SRC "src")

add_executable(triangle ${TRIANGLE_SRC})

cth_target_enable_build_cache(triangle OPTIONAL)

target_link_libraries(triangle PRIVATE cgfw jvk)

# the shaders: GLSL objects, the program reads them from the object folder
cth_glob(TRIANGLE_GLSL "shaders" PATTERNS *.vert *.frag)
add_library(triangle_shaders OBJECT ${TRIANGLE_GLSL})
add_dependencies(triangle triangle_shaders)

# the shaders of one source folder share one object folder
set(TRIANGLE_SHADER_DIR "$<PATH:GET_PARENT_PATH,$<LIST:GET,$<TARGET_OBJECTS:triangle_shaders>,0>>")
target_compile_definitions(triangle PRIVATE TRIANGLE_SHADER_DIR="${TRIANGLE_SHADER_DIR}")
