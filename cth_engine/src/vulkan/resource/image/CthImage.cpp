#include "CthImage.hpp"


#include "vulkan/base/CthDevice.hpp"
#include "vulkan/base/CthPhysicalDevice.hpp"
#include "vulkan/pipeline/CthPipelineBarrier.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/resource/buffer/CthBasicBuffer.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>

#include <stb_image.h>

#include "../CthDeletionQueue.hpp"
#include "../CthMemory.hpp"



namespace cth {
using namespace std;


Image::Image(Device* device, DeletionQueue* deletion_queue, const VkExtent2D extent, const BasicImage::Config& config,
    const VkMemoryPropertyFlags memory_properties) : BasicImage(device, extent, config), deletionQueue(deletion_queue) {
    BasicMemory* memory = new Memory(device, deletionQueue, memory_properties);
    DeletionQueue::debug_check(deletion_queue);

    BasicImage::create();
    BasicImage::alloc(memory);
    BasicImage::bind();
}
Image::~Image() {
    if(get() != VK_NULL_HANDLE) Image::destroy();
}
void Image::wrap(VkImage vk_image, const State& state) {
    if(get() != VK_NULL_HANDLE) destroy();
    destroyMemory();

    BasicImage::wrap(vk_image, state);
}

void Image::create() {
    if(get() != VK_NULL_HANDLE) destroy();
    BasicImage::create();
}



void Image::destroy(DeletionQueue* deletion_queue) {
    CTH_WARN(get() == VK_NULL_HANDLE, "image not valid");

    if(!deletion_queue) {
        destroyMemory();
        BasicImage::destroy(deletionQueue);
    } else {
        destroyMemory(deletion_queue);
        BasicImage::destroy(deletion_queue);
        deletionQueue = deletion_queue;
    }

    BasicImage::reset();
}
void Image::setMemory(BasicMemory* new_memory) {
    CTH_ERR(new_memory == state_.memory, "new_memory must not be current memory") throw details->exception();
    destroyMemory();
    BasicImage::setMemory(new_memory);
}
void Image::destroyMemory(DeletionQueue* deletion_queue) {
    if(state_.memory == nullptr) return;
    if(deletion_queue) state_.memory->free(deletion_queue);
    else state_.memory->free();
    delete state_.memory;
    state_.memory = nullptr;
}


} // namespace cth
