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

    create();
    allocate(memory);
    bind();
}
Image::Image(Device* device, DeletionQueue* deletion_queue, const VkExtent2D extent, const Config& config, unique_ptr<BasicMemory> memory) :
    BasicImage(device, extent, config), deletionQueue(deletion_queue) {
    DeletionQueue::debug_check(deletion_queue);

    create();
    if(!memory->allocated()) allocate(memory.get());
    bind(memory.release());
}



Image::~Image() {
    Image::destroy();
}



uint32_t Image::evalMipLevelCount(const VkExtent2D extent) {
    return static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height))) + 1);
}
void Image::destroy() {
    BasicImage::destroy(deletionQueue);
    memory()->free();

    BasicImage::reset();
}


} // namespace cth
