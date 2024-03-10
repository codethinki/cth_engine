#pragma once
#include <cth/cth_log.hpp>

#include "CthBuffer.hpp"
#include "HlcMemoryChunk.hpp"


#include "..\core\CthDevice.hpp"
#include "../objects/HlcRenderObject.hpp"

//TEMP refactor this
//namespace cth {
//struct FrameInfo;
//
//template<render_type T>
//struct MemoryChunkInfo : MemoryChunkInfoBase {
//    MemoryChunkInfo(const string& unique_name, bool dynamic, uint32_t size);
//};
//
///**
// * \brief the MemoryManager controls and updates the "chunks" of memory allocated
// */
//class MemoryManager {
//public:
//    explicit MemoryManager(Device& device);
//
//    template<render_type T>
//    void registerMemoryChunk(MemoryChunkInfo<T>& chunk_info); //TEMP implement this too
//
//    void allocateBuffers();
//    void bind(RenderObject::Render_Group render_group, const FrameInfo& frame_info) const;
//
//    [[nodiscard]] uint32_t getMemoryChunkId(const string& name);
//    template<render_type T>
//    [[nodiscard]] MemoryChunk<T>* getMemoryChunk(uint32_t id);
//
//
//    [[nodiscard]] int valid_render_groups() const { return validRenderGroups; }
//
//    constexpr static size_t MAX_MEMORY_CHUNKS = 100;
//
//private:
//    template<render_type T>
//    [[nodiscard]] MemoryChunk<T>* newMemoryChunk(const string& unique_name, bool dynamic, uint32_t size);
//    //TEMP left off here. change this to accept the MemoryChunkInfo struct
//
//
//    template<render_type T>
//    span<T> getMemorySpan(bool dynamic, uint32_t size);
//
//    template<typename T>
//    enable_if_t<_Is_any_of_v<T, vertex_t, index_t>, void>
//    static deallocateVectorMemory(vector<T>& vec);
//
//    int validRenderGroups = RenderObject::RENDER_GROUP_INVALID;
//
//    Device& device;
//
//
//    vector<MemoryChunkBase> memoryChunks = []() {
//        vector<MemoryChunkBase> vec{};
//        vec.reserve(MAX_MEMORY_CHUNKS);
//        return vec;
//    }(); //vector with max chunks reserved
//    unordered_map<string, uint32_t> memoryChunkIds = [] {
//        unordered_map<string, uint32_t> map{};
//        map.reserve(MAX_MEMORY_CHUNKS);
//        return map;
//    }(); //map with max chunks reserved
//
//    array<vector<char>, Memory_Type::MEMORY_TYPE_SIZE * 2> memoryBuffers{};
//};
////TODO clear the staged vectors
//
//}
