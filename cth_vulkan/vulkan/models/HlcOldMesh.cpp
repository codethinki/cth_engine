#include "HlcOldMesh.hpp"

namespace cth {

MeshContainer::MeshContainer(const vector<Vertex>& vertices, const vector<uint32_t>& indices) : vertices{vertices}, indices{indices} {
    mergedPositions.emplace_back(0, vertices.size(), 0, indices.size());
}

template<size_t V, size_t I> void MeshContainer::push(const Mesh<V, I>& other) {
    mergedPositions.emplace_back(vertices.size(), vertices.size() + other.vertices.size(), indices.size(), indices.size() + other.indices.size());

    vertices.insert(vertices.end(), other.vertices.begin(), other.vertices.end());
    indices.insert(indices.end(), other.indices.begin(), other.indices.end());
}


void MeshContainer::merge(const MeshContainer& other) {
    mergedPositions.resize(mergedPositions.size() + other.mergedPositions.size());
    uint32_t count = static_cast<uint32_t>(mergedPositions.size());
    const uint32_t indicesCount = static_cast<uint32_t>(indices.size()), verticesCount = static_cast<int>(vertices.size());

    for(MergedPos pos : other.mergedPositions) {
        pos.firstVertex += verticesCount;
        pos.firstIndex += indicesCount;
        mergedPositions[count++] = pos;
    }


    mergedPositions.insert(mergedPositions.end(), other.mergedPositions.begin(), other.mergedPositions.end());
    vertices.insert(vertices.end(), other.vertices.begin(), other.vertices.end());
    indices.insert(indices.end(), other.indices.begin(), other.indices.end());
}

}
