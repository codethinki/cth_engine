#pragma once

#include <span>
#include <string>
#include <glm/ext/scalar_uint_sized.hpp>

#include <cth/cth_log.hpp>

#include "../models/HlcVertex.hpp"


namespace cth {
using namespace std;
template<typename T>
concept chunk_memory_type = _Is_any_of_v<T, char, vertex_t, index_t, instance_t>;

enum Memory_Type {
    MEMORY_TYPE_UNDEFINED,
    MEMORY_TYPE_VERTEX,
    MEMORY_TYPE_INDEX,
    MEMORY_TYPE_INSTANCE,
    MEMORY_TYPE_SIZE
}; //memory type list

template<chunk_memory_type T>
constexpr Memory_Type MEMORY_TYPE_TO_ENUM() {
    if constexpr(is_same_v<T, char>) return Memory_Type::MEMORY_TYPE_UNDEFINED;
    else if constexpr(is_same_v<T, vertex_t>) return Memory_Type::MEMORY_TYPE_VERTEX;
    else if constexpr(is_same_v<T, index_t>) return Memory_Type::MEMORY_TYPE_INDEX;
    else if constexpr(is_same_v<T, instance_t>) return Memory_Type::MEMORY_TYPE_INSTANCE;

    CTH_ASSERT(false && "MEMORY_TYPE_TO_ENUM: invalid / unknown memory type");
    return Memory_Type::MEMORY_TYPE_SIZE;
};

constexpr size_t MEMORY_TYPE_TO_BYTE_SIZE(const Memory_Type type) {
    switch(type) {
        case MEMORY_TYPE_UNDEFINED:
            return sizeof(char);
        case MEMORY_TYPE_VERTEX:
            return sizeof(vertex_t);
        case MEMORY_TYPE_INDEX:
            return sizeof(index_t);
        case MEMORY_TYPE_INSTANCE:
            return sizeof(instance_t);
        default:
            // ReSharper disable once CppStaticAssertFailure
            CTH_ASSERT(false && "MEMORY_TYPE_TO_BYTE_SIZE: type has no size / missing");
            return 0;
    }
};


class MemoryManager;
class MemoryChunk;

struct MemoryChunkCreateInfo {
    string uniqueName; //UNIQUE in the WHOLE PROGRAM
    uint32_t size; //in memory type elements
    bool dynamic; //dynamic memory
    Memory_Type type; //type of the memory contained

private:
    char* memoryPtr = nullptr; //set by MemoryManager
    uint32_t id = UINT32_MAX; //set by MemoryManager

    friend MemoryManager;
    friend MemoryChunk;
};

class MemoryChunk {
public:
    explicit MemoryChunk(const MemoryChunkCreateInfo& create_info);

    template<chunk_memory_type T>
    void write(span<T> data, uint32_t offset);


    template<chunk_memory_type T>
    span<T> memory_span();
    template<chunk_memory_type T>
    uint32_t size() const { return memorySpan.size() / sizeof(T); }

    bool dynamic_memory() const { return dynamic; }
    Memory_Type memory_type() const { return memoryType; }
    string unique_name() const { return uniqueName; }

private:
    void updateMemoryLocation(span<char> new_memory_span);

    uint32_t id;
    string uniqueName;

    const bool dynamic;
    const Memory_Type memoryType;
    span<char> memorySpan;
    bool uploadState = false;
    friend MemoryManager;
};
// TEMP add the "InstanceMemoryChunk " type



// class MemoryChunkOld {
//public:
//    enum Memory_Type {
//        MEMORY_TYPE_VERTEX,
//        MEMORY_TYPE_INDEX,
//        MEMORY_TYPE_INSTANCE,
//        MEMORY_TYPE_SIZE
//    }; //memory type list
//    /**
//     * \param manager ptr to memory manager
//     * \param id of memory chunk (unique)
//     * \param name name of memory chunk (unique)
//     * \param type memory type
//     * \param size chunk size in bytes
//     * \param dynamic dynamic gpu memory? i.e host coherent & host visible?
//     * \param keep_uploaded_data keep copy of to gpu uploaded data?
//     */
//    MemoryChunkOld(MemoryManager* manager, uint32_t id, const string& name, Memory_Type type, uint32_t size, bool dynamic = false,
//        bool keep_uploaded_data = false);
//
//
//    /**
//     * \brief writes data to chunk memory
//     * \param data data in bytes
//     */
//    void write(const span<char>& data) const;
//    /**
//     * \brief writes data to chunk memory
//     * \param data data in bytes
//     * \param data_first first byte of data
//     * \param chunk_memory_first first byte of chunk memory
//     * \param update_size update size in bytes
//     */
//    void write(const span<char>& data, uint32_t data_first, uint32_t chunk_memory_first, uint32_t update_size) const;
//
//protected:
//    MemoryManager* manager;
//    const uint32_t id;
//
//    const string name;
//    const Memory_Type type;
//    const uint32_t size;
//
//    const bool dynamic;
//    const bool keepUploadedData;
//
//    bool uploaded = false;
//
//    vector<char> memoryVector;
//
//    span<char> memoryPtr;
//    char* rawMemoryPtr = nullptr;
//    uint32_t rawMemoryOffset = 0;
//
//private:
//    void setDynamicMemoryPtr(char* ptr, uint32_t first);
//
//    friend MemoryManager;
//};
//
//class StaticMemoryChunkOld : public MemoryChunkOld {
//public:
//    StaticMemoryChunkOld(MemoryManager* manager, uint32_t id, const string& name, Memory_Type type, uint32_t size, bool keep_uploaded_data = false);
//
//    [[nodiscard]] const vector<char>* getMemoryPtr() const;
//};
//
//class DynamicMemoryChunkOld : public MemoryChunkOld {
//public:
//    DynamicMemoryChunkOld(MemoryManager* manager, uint32_t id, const string& name, Memory_Type type, uint32_t size);
//
//    [[nodiscard]] span<char>* getMemoryPtr();
//};



} //TEMP left off here complete this class, update memory manager and complete model render function
