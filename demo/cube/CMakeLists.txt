find_package(glm CONFIG REQUIRED)

cth_glob_cpp(CUBE_SRC "src")

add_executable(cube ${CUBE_SRC})

cth_target_enable_build_cache(cube OPTIONAL)

target_link_libraries(cube PRIVATE cgfw jvk timgui glm::glm-header-only)

target_compile_definitions(cube PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)

# the shaders: GLSL objects, the program reads them from the object folder
# cube.glsl is an include of them and no object
cth_glob(CUBE_GLSL "shaders" PATTERNS *.vert *.frag *.glsl)
add_library(cube_shaders OBJECT ${CUBE_GLSL})
# <jvk/heaps.glsl>
target_link_libraries(cube_shaders PRIVATE jvk)
add_dependencies(cube cube_shaders)

# the shaders of one source folder share one object folder
set(CUBE_SHADER_DIR "$<PATH:GET_PARENT_PATH,$<LIST:GET,$<TARGET_OBJECTS:cube_shaders>,0>>")
target_compile_definitions(cube PRIVATE CUBE_SHADER_DIR="${CUBE_SHADER_DIR}")
