#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

#include "cube.glsl"

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUv;

void main() {
    // the index buffer gives the vertex index, the vertices come through their address
    Vertex vertex = draw.vertices.vertices[gl_VertexIndex];
    gl_Position = draw.mvp * vec4(vertex.position, 1.0);
    outNormal = draw.normal * vertex.normal;
    outUv = vertex.uv;
}
