#pragma once
import hlc.typedefs;
import hlc.math;

#include<array>
#include<vector>

#include "HlcVertex.hpp"


namespace cth {
using namespace std;

template<size_t V, size_t I> struct Mesh {
	array<Vertex, V> vertices{};
	array<uint32_t, I> indices{};
};

struct MeshContainer {
	MeshContainer() = default;
	MeshContainer(const vector<Vertex>& vertices, const vector<uint32_t>& indices);

	template<size_t V, size_t I>
	void push(const Mesh<V, I>& other);

	void merge(const MeshContainer& other);


	[[nodiscard]] bool empty() const { return vertices.empty() && indices.empty(); }

	struct MergedPos {
		uint32_t firstVertex, vertices, firstIndex, indices;
	};
	vector<MergedPos> mergedPositions{};
	vector<Vertex> vertices{};
	vector<uint32_t> indices{};
};


namespace mesh_templates {
	constexpr Mesh<4, 6> line(const glm::vec3& a, const glm::vec3& b, const float line_width = 0.002f) {
		float dy = b.y - a.y;

		const float nInvSlope = (a.x - b.x) / (math::abs(dy) < 1.e-6f ? -1.e-6f : dy);
		const float dx = line_width / math::heronSqrt<float>(1 + nInvSlope * nInvSlope);
		dy = dx * nInvSlope; //calculates offset angle and offsets for x and y like |-----| 

		return Mesh{
			array<Vertex, 4>{
				Vertex{{a.x - dx, a.y - dy, a.z}}, //t left
				Vertex{{b.x - dx, b.y - dy, a.z}}, //t right
				Vertex{{b.x + dx, b.y + dy, a.z}}, //b right
				Vertex{{a.x + dx, a.y + dy, a.z}} //b left
			},
			array<uint32_t, 6>{0, 1, 2, 2, 3, 0}};
	}
	inline Mesh<4, 6> rect(const glm::vec3& a, const glm::vec3& b) {
		return Mesh{
			array<Vertex, 4>{
				Vertex{{a.x, a.y, a.z}}, //t left
				Vertex{{b.x, a.y, a.z}}, //t right
				Vertex{{b.x, b.y, a.z}}, //b right
				Vertex{{a.x, b.y, a.z}} //b left
			},
			array<uint32_t, 6>{0, 1, 2, 2, 3, 0}
		};
	}
}

}