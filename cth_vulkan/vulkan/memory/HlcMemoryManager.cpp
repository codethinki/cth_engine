#include "HlcMemoryManager.hpp"

#include <cth/cth_log.hpp>

#include "HlcMemoryChunk.hpp"

#include "../objects//HlcRenderObject.hpp"
#include "../utils/HlcFrameInfo.hpp"


namespace cth {
template<render_type T>
MemoryChunkInfo<T>::MemoryChunkInfo(const string& unique_name, bool dynamic, uint32_t size) : MemoryChunkInfo(unique_name, dynamic,
    MEMORY_TYPE_TO_ENUM<T>(), size) {}



MemoryManager::MemoryManager(Device& device) : device(device) {}

uint32_t MemoryManager::getMemoryChunkId(const string& name) {
    CTH_ASSERT(memoryChunkIds.contains(name) && "getMemoryChunkId: memory chunk not registered");
    return memoryChunkIds[name];
}


template<render_type T>
MemoryChunk<T>* MemoryManager::getMemoryChunk(const uint32_t id) {
    CTH_ASSERT(memoryChunks[id].type != MEMORY_TYPE_TO_ENUM<T>() && "getMemoryChunk: type mismatch (T != chunk.type)");
    return static_cast<T*>(&memoryChunks[id]);
}


//void MemoryManager::allocateBuffers() {
//	if(staticVertices.size()) {
//		staticVertexBuffer = make_unique<Buffer>(device, sizeof(Vertex), staticVertices.size(),
//			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
//
//		staticVertexBuffer->stage(staticVertices.data());
//		deallocateVectorMemory(staticVertices);
//
//		allowedRenderGroups |= RenderObject::RENDER_GROUP_STATIC_VERTICES | RenderObject::RENDER_GROUP_STATIC;
//	}
//	if(staticIndices.size()) {
//		staticIndexBuffer = make_unique<Buffer>(device, sizeof(uint32_t), staticIndices.size(),
//			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
//
//		staticIndexBuffer->stage(staticIndices.data());
//		deallocateVectorMemory(staticIndices);
//
//		allowedRenderGroups |= RenderObject::RENDER_GROUP_STATIC_INDICES;
//	}
//	else allowedRenderGroups &= ~RenderObject::RENDER_GROUP_STATIC;
//
//	if(dynamicVertices.size()) {
//		dynamicVertexBuffer = make_unique<Buffer>(device, sizeof(Vertex), dynamicVertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
//			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
//
//		dynamicVertexBuffer->map();
//		dynamicVertexBuffer->writeToBuffer(dynamicVertices.data());
//
//		allowedRenderGroups |= RenderObject::RENDER_GROUP_DYNAMIC;
//	}
//	else allowedRenderGroups &= ~RenderObject::RENDER_GROUP_STATIC_INDICES;
//
//	if(dynamicIndices.size()) {
//		dynamicIndexBuffer = make_unique<Buffer>(device, sizeof(uint32_t), dynamicIndices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
//			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
//
//		dynamicIndexBuffer->map();
//		dynamicIndexBuffer->writeToBuffer(dynamicIndices.data());
//	}
//	else {
//		allowedRenderGroups &= ~RenderObject::RENDER_GROUP_DYNAMIC;
//		allowedRenderGroups &= ~RenderObject::RENDER_GROUP_STATIC_VERTICES;
//	}
//
//}

//void MemoryManager::bind(const RenderObject::Render_Group render_group, const FrameInfo& frame_info) const {
//	assert(allowedRenderGroups & render_group && "prohibited render group called");
//
//	constexpr VkDeviceSize offsets[] = {0};
//	array<VkBuffer, 1> buffersV;
//	VkBuffer bufferI;
//	if(render_group & RenderObject::RENDER_GROUP_STATIC ||
//		render_group & RenderObject::RENDER_GROUP_STATIC_VERTICES)
//		buffersV = {staticVertexBuffer->getBuffer()};
//	else buffersV = {dynamicVertexBuffer->getBuffer()};
//
//
//	if(render_group & RenderObject::RENDER_GROUP_STATIC ||
//		render_group & RenderObject::RENDER_GROUP_STATIC_INDICES)
//		bufferI = staticIndexBuffer->getBuffer();
//	else bufferI = dynamicIndexBuffer->getBuffer();
//
//	vkCmdBindVertexBuffers(frame_info.commandBuffer, 0, 1, buffersV.data(), offsets);
//	vkCmdBindIndexBuffer(frame_info.commandBuffer, bufferI, 0, VK_INDEX_TYPE_UINT32);
//
//	//IMPLEMENT needs to be updated
//}


/*void MemoryManager::bind(RenderObject::Render_Group render_group, const FrameInfo& frame_info) const {
CTH_ASSERT(allowedRenderGroups & render_group && "prohibited render group called");
}*/



template<render_type T>
MemoryChunk<T>* MemoryManager::newMemoryChunk(const string& unique_name, bool dynamic, uint32_t size) {
    CTH_ASSERT(!memoryChunkIds.contains(unique_name) && "newMemoryChunk: memory chunk already exists");
    CTH_ASSERT(memoryChunks.size() < MAX_MEMORY_CHUNKS && "newMemoryChunk: max chunk count reached");

    memoryChunkIds[unique_name] = memoryChunks.size();

    const span<T> chunkMemory = getMemorySpan<T>(dynamic, size);

    memoryChunks.emplace_back(MemoryChunk<T>(memoryChunks.size(), unique_name, dynamic, MEMORY_TYPE_TO_ENUM<T>(), chunkMemory));

    return static_cast<T*>(&memoryChunks.back());
}


template<render_type T>
span<T> MemoryManager::getMemorySpan(const bool dynamic, uint32_t size) {
    const int memoryTypeValue = MEMORY_TYPE_TO_ENUM<T>();
    const uint32_t allocByteSize = sizeof(T) * size;
    auto& buffer = memoryBuffers[Memory_Type::MEMORY_TYPE_SIZE * dynamic + memoryTypeValue];

    CTH_ASSERT(buffer.size() + allocByteSize < buffer.capacity() && "getMemorySpan: exceeding buffer size");

    buffer.push_back(allocByteSize);

    return span<T>{static_cast<T*>(&buffer.back()), size};
}

template<typename T>
enable_if_t<_Is_any_of_v<T, vertex_t, index_t>, void> MemoryManager::deallocateVectorMemory(vector<T>& vec) {
    vec.clear();
    vec.shrink_to_fit();
}

}
