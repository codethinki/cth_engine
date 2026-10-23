#include "jolly/render/submit/queue.hpp"

#include "jolly/render/submit/submit_info.hpp"

#include "jvk/base/queue/queue.hpp"
#include "jvk/base/queue/queue_family.hpp"

#include "jolly/utility/jvk_conversion.hpp"


namespace jly {

namespace {
    constexpr QueueProperties to_queue_properties(jvk::QueueFamilyProperties properties) {
        QueueProperties out{};
        using jvk::QueueFamilyProperties;


        if(contains(properties, QueueFamilyProperties::GRAPHICS)) out |= QueueProperties::GRAPHICS;
        if(contains(properties, QueueFamilyProperties::TRANSFER)) out |= QueueProperties::TRANSFER;
        if(contains(properties, QueueFamilyProperties::COMPUTE)) out |= QueueProperties::COMPUTE;
        if(contains(properties, QueueFamilyProperties::PRESENT)) out |= QueueProperties::PRESENT;

        return out;
    }
}

Queue::Queue() = default;
Queue::Queue(std::unique_ptr<jvk::Queue> vk_queue) : _handle{std::move(vk_queue)} {}


Queue::~Queue() = default;

void Queue::wrap(std::unique_ptr<jvk::Queue> handle) { _handle = std::move(handle); }

void Queue::destroy() { _handle->destroy(); }

Queue::State Queue::release() { return {std::move(_handle)}; }
void Queue::submit(SubmitInfo& info) const { _handle->const_submit(info.next().raw()); }

void Queue::skip(SubmitInfo& info) const { _handle->const_skip(info.next().raw()); }

void Queue::wait() const { _handle->wait(); }

bool Queue::created() const { return _handle->created(); }

uint32_t Queue::index() const { return _handle->index(); }

uint32_t Queue::familyIndex() const { return _handle->familyIndex(); }

QueueProperties Queue::familyProperties() const { return to_queue_properties(_handle->familyProperties()); }

Queue::Queue(Queue&&) noexcept = default;

Queue& Queue::operator=(Queue&&) noexcept = default;

}
