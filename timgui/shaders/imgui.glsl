// the vertex and the push data of the ImGui draws, mirrored by `ImDrawVert` and `dev::Draw` in imgui.cpp
// needs GL_EXT_buffer_reference in the including shader
#ifndef TIMGUI_IMGUI_GLSL
#define TIMGUI_IMGUI_GLSL

// `ImDrawVert`: only scalars, std430 gives the stride of 20 bytes without the scalar block layout
struct Vertex {
    float x;
    float y;
    float u;
    float v;
    uint color; // RGBA8, R in the lowest byte
};

layout(buffer_reference, std430) readonly buffer Vertices {
    Vertex vertices[];
};

layout(push_constant, std430) uniform Draw {
    Vertices vertices;
    vec2 scale; // display to clip space
    vec2 translate;
    uint textureIdx;
    uint samplerIdx;
} draw;

#endif
