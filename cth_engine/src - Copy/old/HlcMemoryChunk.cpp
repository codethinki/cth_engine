#include "HlcMemoryChunk.hpp"

#include <cth/cth_log.hpp>

#include <algorithm>
#include <assert.h>
#include <iostream>


namespace cth {

//MemoryChunk::MemoryChunk(const MemoryChunkCreateInfo& create_info) : id(create_info.id), uniqueName(create_info.uniqueName),
//    dynamic(create_info.dynamic), memoryType(create_info.type), memorySpan{create_info.memoryPtr, create_info.size} {
//    CTH_WARN(create_info.type != MEMORY_TYPE_UNDEFINED && "MemoryChunk: chunk with undefined type created");
//}
//void MemoryChunk::updateMemoryLocation(const span<char> new_memory_span) {
//    CTH_ASSERT(new_memory_span.size() == memorySpan.size() && "updateMemoryLocation: memory span size changed");
//    memorySpan = new_memory_span;
//}
//
//template<chunk_memory_type T>
//void MemoryChunk::write(span<T> data, const uint32_t offset) {
//    CTH_ASSERT(memorySpan.size() <= data.size_bytes() + (offset) * sizeof(T) && "write: memory overflow");
//
//    if constexpr(is_same_v<T, char>) copy_n(data.begin(), data.size(), memorySpan.begin() + offset);
//    else write(span<char>{reinterpret_cast<char*>(data.data()), data.size_bytes()}, offset * sizeof(T));
//}
//
//template<chunk_memory_type T>
//span<T> MemoryChunk::memory_span() {
//    CTH_ASSERT((MEMORY_TYPE_TO_ENUM<T>() == memoryType || is_same_v<T, char>) && "memory_span: type mismatch!");
//
//    return span<T>{reinterpret_cast<T*>(memorySpan.data()), memorySpan.size() / sizeof(T)};
//}

//TEMP left off here. make the memory manager use this new memory chunk

//TEMP clean up this file

/*MemoryChunkOld::MemoryChunkOld(MemoryManager* manager, const uint32_t id, const string& name, const Memory_Type type, const uint32_t size,
   const bool dynamic, const bool keep_uploaded_data) : manager(manager), id(id), name(name), type(type), size(size), dynamic(dynamic),
   keepUploadedData(keep_uploaded_data), memoryVector(size) {}


void MemoryChunkOld::write(const span<char>& data) const {
   WARN(data.size() != size && "write: data size is not equal to storage size");

   write(data, 0, 0, data.size());
}
void MemoryChunkOld::write(const span<char>& data, const uint32_t data_first, const uint32_t chunk_memory_first, const uint32_t update_size) const {
   assert(update_size != 0 && "write: update size = 0");
   assert(data.size() - data_first - update_size > 0 && "write: data too small to write");
   assert(memoryVector.size() - chunk_memory_first - update_size > 0 && "write: chunk size too small to write data");

   assert(!dynamic && !uploaded || dynamic && "write: cant write to uploaded static chunk");

   if(uploaded && dynamic) {
       HINT(false && "write: copying data to uploaded dynamic chunk. Consider writing directly to memoryPtr");
       copy_n(data.begin() + data_first, update_size, memoryPtr.begin() + chunk_memory_first);
   }
   else copy_n(data.begin() + data_first, update_size, memoryVector.begin() + chunk_memory_first);
}


void MemoryChunkOld::setDynamicMemoryPtr(char* ptr, const uint32_t first) {
   memoryPtr = {ptr + first, size};
   rawMemoryPtr = ptr;
   rawMemoryOffset = first;
}



StaticMemoryChunkOld::StaticMemoryChunkOld(MemoryManager* manager, const uint32_t id, const string& name, const Memory_Type type, const uint32_t size,
   const bool keep_uploaded_data): MemoryChunkOld(manager, id, name, type, size, false, keep_uploaded_data) {}



const vector<char>* StaticMemoryChunkOld::getMemoryPtr() const {
   assert(uploaded && "getMemoryPtr: no static memory pointer ready before uploading");

   return &memoryVector;
}




DynamicMemoryChunkOld::DynamicMemoryChunkOld(MemoryManager* manager, const uint32_t id, const string& name, const Memory_Type type,
   const uint32_t size): MemoryChunkOld(manager, id, name, type, size, true, false) {}

span<char>* DynamicMemoryChunkOld::getMemoryPtr() {
   assert(uploaded && "getMemoryPtr: no memory pointer ready before uploading");
   assert(rawMemoryPtr != nullptr && "getMemoryPtr: rawMemoryPointer not set");

   return &memoryPtr;
}*/
}
