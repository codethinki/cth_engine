#pragma once
#include "../memory/HlcMemoryChunkList.hpp"

#include <cstdint>


namespace cth {
class MemoryManager;

enum class Memory_Chunk_Type { VERTEX, INDEX, INSTANCE, INVALID };
enum class Memory_Chunk_State { STATIC, DYNAMIC };


struct MemoryChunkBase {
    MemoryChunkBase(const Memory_Chunk_Type type, const Memory_Chunk_State state, const uint32_t size, const uint32_t bytes, const uint32_t id,
        const uint32_t first_index, const uint32_t first_byte, void* memory) : type(type),
        size(size), bytes(bytes), id(id), firstIndex(first_index), firstByte(first_byte), state(state),
        memory(memory) {}

    const Memory_Chunk_Type type;
    const uint32_t size; /*size in elements*/
    const uint32_t bytes; /*size in bytes*/

    const uint32_t id;

    [[nodiscard]] uint32_t getFirstIndex() const { return firstIndex; }
    [[nodiscard]] uint32_t getFirstByte() const { return firstByte; }

private:
    void changeDataLocation(const uint32_t first_index, const uint32_t first_byte, void* memory) {
        this->memory = memory;
        firstIndex = first_index;
        firstByte = first_byte;
    }
    uint32_t firstIndex; /*first Index of this chunk in buffer*/
    uint32_t firstByte; /*first byte of this chunk in buffer*/

    const Memory_Chunk_State state;

protected:
    void* memory;

    friend MemoryManager;
};
struct StaticMemoryChunk : public MemoryChunkBase {
    StaticMemoryChunk(const Memory_Chunk_Type type, const uint32_t size, const uint32_t bytes, const uint32_t id,
        const uint32_t first_index, const uint32_t first_byte, void* memory) : MemoryChunkBase(
        type, Memory_Chunk_State::STATIC, size, bytes, id, first_index, first_byte, memory) {}

    [[nodiscard]] const void* ptr() const { return memory; }
};
struct DynamicMemoryChunk : public MemoryChunkBase {
    DynamicMemoryChunk(const Memory_Chunk_Type type, const uint32_t size, const uint32_t bytes, const uint32_t id,
        const uint32_t first_index, const uint32_t first_byte, void* memory) : MemoryChunkBase(
        type, Memory_Chunk_State::DYNAMIC, size, bytes, id, first_index, first_byte, memory) {}

    [[nodiscard]] void* ptr() const { return memory; }
};


//enum class Memory_Chunk_Type { VERTEX_STATIC, VERTEX_DYNAMIC, INDEX_STATIC, INDEX_DYNAMIC, INVALID };
//
//struct MemoryChunkStorageInfo {
//	Memory_Chunk_List chunkName = Memory_Chunk_List::NONE; /*index from "Memory_Chunk_List"*/
//	Memory_Chunk_Type type = Memory_Chunk_Type::INVALID; /*memory type: VERTEX / INDEX _ STATIC/DYNAMIC*/
//	uint32_t firstIndex = 0; /*first element*/
//	uint32_t size = 0; /*element count*/
//};
//
//struct MemoryChunkAllocInfo {
//	/**
//	 * \param chunk_name index from "Memory_Chunk_List"
//	 * \param type memory type: VERTEX / INDEX _ STATIC/DYNAMIC
//	 * \param new_chunk true: new memory, false: shared if possible
//	 * \param size chunk size in element count, specify 0 for read-only memory
//	 */
//	MemoryChunkAllocInfo(const Memory_Chunk_List chunk_name, const Memory_Chunk_Type type, const bool new_chunk, const uint32_t size = 0) : chunkName(
//			chunk_name), type(type),
//		newChunk(new_chunk), size(size) { assert((!new_chunk || (new_chunk && size != 0)) && "invalid chunkAllocInfo: size of new chunk has to be > 0"); }
//
//
//	Memory_Chunk_List chunkName = Memory_Chunk_List::NONE;
//	Memory_Chunk_Type type = Memory_Chunk_Type::INVALID;
//	bool newChunk = false;
//	uint32_t size = 0;
//};
//
}
