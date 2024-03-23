#pragma once
#include "CthBuffer.hpp"

#include "HlcChunkUpdateQueue.hpp"
#include "HlcMemoryChunkList.hpp"
#include "../core/CthDevice.hpp"
#include "../models/HlcMesh.hpp"
#include "../objects/HlcRenderObject.hpp"


namespace cth {
struct FrameInfo;

struct MemoryChunkStorageInfo;

/**
 * \brief the OldMemoryManager controls and updates the "chunks" of memory allocated
 */
class OldMemoryManager {
public:
	explicit OldMemoryManager(Device& device) : device(device) {}
	/**
	 * \brief allocates objects vkBufferMemory and queries memory chunk info ->  will call "receiveChunkInfo" on objects, calls "writeChunkData" for non-read-only memory
	 */
	void allocate(const vector<unique_ptr<RenderObject>>& objects);
	void allocateBuffers();
	void bind(RenderObject::Render_Group render_group, const FrameInfo& frame_info) const;
	void updateDynamicChunks();

	inline static array<bool, static_cast<size_t>(Memory_Chunk_List::SIZE)> allocatedChunks{};
	/*true if at least one vertex chunk of a model has been allocated*/

	int allowedRenderGroups = RenderObject::RENDER_GROUP_INVALID;

private:
	template<typename T> enable_if_t<is_same_v<decay_t<T>, Vertex> || is_same_v<decay_t<T>, uint32_t>, void>
	static deallocateVectorMemory(vector<T>& vec);

	template<typename T>
	static void readChunkUpdateQueue(const vector<uint32_t>& queue, Buffer* buffer, const vector<T>& storage);

	Device& device;

	vector<Vertex> staticVertices;
	vector<uint32_t> staticIndices;

	vector<Vertex> dynamicVertices{};
	vector<uint32_t> dynamicIndices{};

	ChunkUpdateQueue updateQueue{&dynamicVertices, &dynamicIndices};

	vector<MemoryChunkStorageInfo> storageInfo{static_cast<size_t>(Memory_Chunk_List::SIZE)};
	/*first info will be at its model index, rest undefined*/


	unique_ptr<Buffer> staticVertexBuffer;
	unique_ptr<Buffer> staticIndexBuffer;
	unique_ptr<Buffer> dynamicVertexBuffer;
	unique_ptr<Buffer> dynamicIndexBuffer;
};


}
//TEMP remove file