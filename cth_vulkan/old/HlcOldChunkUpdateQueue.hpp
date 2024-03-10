#pragma once
import hlc.algorithm;

#include "../models/HlcVertex.hpp"

#include <vector>


namespace cth {
using namespace std;

class HlcMemoryManager;

struct VerticesCUQ {
	explicit VerticesCUQ(vector<Vertex>* vertices_storage) : verticesStorage(vertices_storage) {}
	virtual ~VerticesCUQ() = 0;

	virtual void queueVerticesUpdate(uint32_t first_vertex, uint32_t size) = 0; //queue buffer update

	vector<Vertex>* verticesStorage = nullptr; // ptr to all dynamic vertices
};
inline VerticesCUQ::~VerticesCUQ() { }

struct IndicesCUQ {
	explicit IndicesCUQ(vector<uint32_t>* indices_storage) : indicesStorage(indices_storage) {}
	virtual ~IndicesCUQ() = 0;

	virtual void queueIndicesUpdate(uint32_t first_index, uint32_t size) = 0; //queue buffer update

	vector<uint32_t>* indicesStorage = nullptr; //ptr to all dynamic indices
};
inline IndicesCUQ::~IndicesCUQ() { }


class ChunkUpdateQueue final : public VerticesCUQ, public IndicesCUQ {
public:
	ChunkUpdateQueue(vector<Vertex>* vertices_storage, vector<uint32_t>* indices_storage) : VerticesCUQ(vertices_storage),
		IndicesCUQ(indices_storage) {}

	/**
	 * \param update_size in vertices count
	 */
	void queueVerticesUpdate(uint32_t first_vertex, uint32_t update_size) override;

	/**
	 * \param update_size indices count
	 */
	void queueIndicesUpdate(uint32_t first_index, uint32_t update_size) override;

	inline static constexpr uint32_t CHUNK_UPDATE_QUEUE_TOLERANCE = 20; //reduce number of buffer updates -> merge nearby updates

private:
	vector<uint32_t> vertexQueue{}; //stores data like this: {..., first index (inclusive), last index (exclusive), ...}
	vector<uint32_t> indexQueue{}; //stores data like this: {..., first index (inclusive), last index (exclusive), ...}
	static void queueUpdate(uint32_t first, uint32_t size, vector<uint32_t>& queue, uint32_t tolerance); //update queue algorithm -> handles overlap / duplication of updates

	friend class MemoryManager; //to read queue
};

} //TEMP remove file