#include "HlcOldMemoryManager.hpp"

#include "../objects//HlcRenderObject.hpp"
#include "../utils/HlcFrameInfo.hpp"


namespace cth {

void OldMemoryManager::allocate(const vector<unique_ptr<RenderObject>>& objects) {
	vector<array<uint32_t, 2>> lastAllocated{};

	for(int i = 0; i < objects.size(); i++) {
		const auto& object = objects[i];
		const auto& chunkAllocInfos = object->chunkAllocInfos;
		assert(chunkAllocInfos.size() == object->chunks && "every object must have defined allocation behaviour");

		for(int k = 0; k < chunkAllocInfos.size(); k++) {
			const auto& chunkAllocInfo = chunkAllocInfos[k];
			if(!chunkAllocInfo.newChunk && chunkAllocInfo.size == 0) {
				lastAllocated.push_back(array<uint32_t, 2>{static_cast<uint32_t>(i), static_cast<uint32_t>(k)});
				continue;
			}

			const int chunkIndex = static_cast<int>(chunkAllocInfo.chunkName), type = static_cast<int>(chunkAllocInfo.type);
			int storageInfoIndex = chunkIndex;

			const bool vertex = type < 2, dynamic = type & 0x1;

			if(chunkAllocInfo.newChunk || !allocatedChunks[chunkIndex]) {
				MemoryChunkStorageInfo chunkStorageInfo{};
				chunkStorageInfo.type = chunkAllocInfo.type;
				chunkStorageInfo.chunkName = chunkAllocInfo.chunkName;
				chunkStorageInfo.size = chunkAllocInfo.size;

				if(vertex) {
					vector<Vertex> chunk(chunkAllocInfo.size);
					object->writeChunkData(k, &chunk, nullptr);

					if(dynamic) {
						chunkStorageInfo.firstIndex = static_cast<uint32_t>(dynamicVertices.size());
						dynamicVertices.insert(dynamicVertices.end(), chunk.begin(), chunk.end());
					}
					else {
						chunkStorageInfo.firstIndex = static_cast<uint32_t>(staticVertices.size());
						staticVertices.insert(staticVertices.end(), chunk.begin(), chunk.end());
					}
				}
				else {
					vector<uint32_t> chunk(chunkAllocInfo.size);
					object->writeChunkData(k, nullptr, &chunk);

					if(dynamic) {
						chunkStorageInfo.firstIndex = static_cast<uint32_t>(dynamicIndices.size());
						dynamicIndices.insert(dynamicIndices.end(), chunk.begin(), chunk.end());
					}
					else {
						chunkStorageInfo.firstIndex = static_cast<uint32_t>(staticIndices.size());
						staticIndices.insert(staticIndices.end(), chunk.begin(), chunk.end());
					}
				}

				if(allocatedChunks[chunkIndex]) {
					storageInfoIndex = static_cast<int>(storageInfo.size());

					storageInfo.push_back(chunkStorageInfo);
				}
				else {
					storageInfo[chunkIndex] = chunkStorageInfo;
					allocatedChunks[chunkIndex] = true;
				}
			}
			assert(chunkAllocInfo.chunkName == storageInfo[storageInfoIndex].chunkName);

			object->receiveChunkInfo(k, storageInfo[storageInfoIndex], dynamic && vertex ? &updateQueue : nullptr,
				dynamic && !vertex ? &updateQueue : nullptr);
		}
	}
	for(const auto& element : lastAllocated) {
		const auto& object = objects[element[0]];
		const auto& allocInfo = object->chunkAllocInfos[element[1]];

		const int type = static_cast<int>(allocInfo.type);
		const bool vertex = type < 2, dynamic = type % 2 == 1;

		assert(allocatedChunks[static_cast<size_t>(allocInfo.chunkName)] && "chunk must be allocated by some object");
		object->receiveChunkInfo(element[1], storageInfo[static_cast<size_t>(allocInfo.chunkName)], dynamic && vertex ? &updateQueue : nullptr,
			dynamic && !vertex ? &updateQueue : nullptr);
	}

}
void OldMemoryManager::allocateBuffers() {
	if(staticVertices.size()) {
		staticVertexBuffer = make_unique<Buffer>(device, sizeof(Vertex), staticVertices.size(),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		staticVertexBuffer->stage(staticVertices.data());
		deallocateVectorMemory(staticVertices);

		allowedRenderGroups |= RenderObject::RENDER_GROUP_STATIC_VERTICES | RenderObject::RENDER_GROUP_STATIC;
	}
	if(staticIndices.size()) {
		staticIndexBuffer = make_unique<Buffer>(device, sizeof(uint32_t), staticIndices.size(),
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		staticIndexBuffer->stage(staticIndices.data());
		deallocateVectorMemory(staticIndices);

		allowedRenderGroups |= RenderObject::RENDER_GROUP_STATIC_INDICES;
	}
	else allowedRenderGroups &= ~RenderObject::RENDER_GROUP_STATIC;

	if(dynamicVertices.size()) {
		dynamicVertexBuffer = make_unique<Buffer>(device, sizeof(Vertex), dynamicVertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		dynamicVertexBuffer->map();
		dynamicVertexBuffer->writeToBuffer(dynamicVertices.data());

		allowedRenderGroups |= RenderObject::RENDER_GROUP_DYNAMIC;
	}
	else allowedRenderGroups &= ~RenderObject::RENDER_GROUP_STATIC_INDICES;

	if(dynamicIndices.size()) {
		dynamicIndexBuffer = make_unique<Buffer>(device, sizeof(uint32_t), dynamicIndices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		dynamicIndexBuffer->map();
		dynamicIndexBuffer->writeToBuffer(dynamicIndices.data());
	}
	else {
		allowedRenderGroups &= ~RenderObject::RENDER_GROUP_DYNAMIC;
		allowedRenderGroups &= ~RenderObject::RENDER_GROUP_STATIC_VERTICES;
	}

}

void OldMemoryManager::bind(const RenderObject::Render_Group render_group, const FrameInfo& frame_info) const {
	assert(allowedRenderGroups & render_group && "prohibited render group called");

	constexpr VkDeviceSize offsets[] = {0};
	array<VkBuffer, 1> buffersV;
	VkBuffer bufferI;
	if(render_group & RenderObject::RENDER_GROUP_STATIC ||
		render_group & RenderObject::RENDER_GROUP_STATIC_VERTICES)
		buffersV = {staticVertexBuffer->getBuffer()};
	else buffersV = {dynamicVertexBuffer->getBuffer()};


	if(render_group & RenderObject::RENDER_GROUP_STATIC ||
		render_group & RenderObject::RENDER_GROUP_STATIC_INDICES)
		bufferI = staticIndexBuffer->getBuffer();
	else bufferI = dynamicIndexBuffer->getBuffer();

	vkCmdBindVertexBuffers(frame_info.commandBuffer, 0, 1, buffersV.data(), offsets);
	vkCmdBindIndexBuffer(frame_info.commandBuffer, bufferI, 0, VK_INDEX_TYPE_UINT32);
}

void OldMemoryManager::updateDynamicChunks() {
	if(updateQueue.vertexQueue.size() > 0) {
		readChunkUpdateQueue(updateQueue.vertexQueue, dynamicVertexBuffer.get(), dynamicVertices);
		updateQueue.vertexQueue.clear();
	}
	if(updateQueue.indexQueue.size() > 0) {
		readChunkUpdateQueue(updateQueue.indexQueue, dynamicIndexBuffer.get(), dynamicIndices);
		updateQueue.indexQueue.clear();
	}

}
template<typename T>
void OldMemoryManager::readChunkUpdateQueue(const vector<uint32_t>& queue, Buffer* buffer, const vector<T>& storage) {
	for(size_t i = 0; i < queue.size(); i += 2) {
		const uint32_t first = queue[i];
		const uint32_t size = queue[i + 1] - first;
		assert(first + size <= storage.size() && "queued update out of bounds");

		const VkDeviceSize offset = first * sizeof(T);
		buffer->writeToBuffer(storage.data(), size * sizeof(T), offset, offset);
	}
}

template<typename T> enable_if_t<is_same_v<decay_t<T>, Vertex> || is_same_v<decay_t<T>, uint32_t>, void>
OldMemoryManager::deallocateVectorMemory(vector<T>& vec) {
	vec.clear();
	vec.shrink_to_fit();
}

}
//TEMP remove file