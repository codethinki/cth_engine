#pragma once
#include "HlcStandardObject.hpp"

#include <string>
#include <vector>

namespace cth::vk {
using namespace std;

struct Vertex;

class Model;


class RenderObject : public StandardObject {
public:
    RenderObject() = default;
    explicit RenderObject(Transform const& transform) : StandardObject(transform) {}
    ~RenderObject() override = 0;

    enum Render_Group {
        RENDER_GROUP_INVALID,
        RENDER_GROUP_STATIC = 1,
        RENDER_GROUP_STATIC_VERTICES = 2,
        RENDER_GROUP_STATIC_INDICES = 4,
        RENDER_GROUP_DYNAMIC = 8,
        RENDER_GROUP_NONE = 16
    };
    int renderGroupFlags = RENDER_GROUP_INVALID; /*objects render groups*/

    /** @param current_group register through setting renderGroupFlags*/
    virtual void render(Render_Group current_group) = 0;


    /**
     * @brief request models to access for drawing
     * @return model names for the requested models
     */
    virtual vector<string> getReqModels() = 0;

    /**
     * @brief receive model pointers for rendering
     */
    virtual void recModelPtrs(vector<Model*> const& model_ptrs) = 0;


    RenderObject(RenderObject const&) = default;
    RenderObject& operator=(RenderObject const&) = default;
    RenderObject(RenderObject&&) = default;
    RenderObject& operator=(RenderObject&&) = default; //copy/move constructors

protected:
    //virtual void setChunkInfos() = 0; /*used to set chunkAllocInfos and chunk count*/
};

inline RenderObject::~RenderObject() {}
//TEMP old code
//class MemoryManager;
//struct MemoryChunkStorageInfo;
//struct MemoryChunkAllocInfo;
//struct VerticesCUQ;
//struct IndicesCUQ;
///**
//     * @brief write the memory for a model to a memory chunk
//     * @param chunk_info_index index of the chunkAllocInfo
//     * @param vertex_storage will have the dimensions specified in the info
//     * @param index_storage will have the dimensions specified in the info
//     */
//    /*virtual void writeChunkData(size_t chunk_info_index, vector<Vertex>* vertex_storage, vector<uint32_t>* index_storage) = 0;*/
//
//    /**
//     * @brief gets called by MeshMemoryManager. Used to store offsets for draw calls and the pointer to the dynamic memory storage
//     * @param chunk_info_index chunk_info_index index of the chunkAllocInfo
//     * @param storage_info info of the data range of the chunk in the buffer memory
//     * @param queue_vertex update queue with pointer to vertex_storage
//     * @param queue_indices update_queue with pointer to index_storage
//     */
//    /*virtual void receiveChunkInfo(size_t chunk_info_index, const MemoryChunkStorageInfo& storage_info, const VerticesCUQ* queue_vertex = nullptr,
//        IndicesCUQ* queue_indices = nullptr) = 0;*/
//
//    //vector<MemoryChunkAllocInfo> chunkAllocInfos{}; /*memory chunk allocation infos, will be allocated in order*/
//    //int chunks = -1; /*number of memory chunks to allocate*/
//
//    /*PushConstants pushConstant{};*/ //old stuff

//
}
