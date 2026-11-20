// the vertex and the push data of the cube, mirrored by `Vertex` and `Draw` in main.cpp
// needs GL_EXT_buffer_reference and GL_EXT_scalar_block_layout in the including shader
#ifndef CUBE_GLSL
#define CUBE_GLSL

struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
};

layout(buffer_reference, scalar) readonly buffer Vertices {
    Vertex vertices[];
};

layout(push_constant, scalar) uniform Draw {
    Vertices vertices;
    mat4 mvp;
    mat3 normal; // the rotation of the model, rotates the normals
    uint textureIdx;
    uint samplerIdx;
} draw;

#endif
