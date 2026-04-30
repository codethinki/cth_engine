#include "jolly/render/submit/queue.hpp"

#include "jolly/render/submit/submit_info.hpp"

#include "jvk/base/queue/queue.hpp"
#include "jvk/base/queue/queue_family.hpp"

#include "jolly/utility/jvk_conversion.hpp"


namespace jly {

namespace {
    constexpr QueueProperty to_queue_properties(jvk::QueueFamilyProperties properties) {
        QueueProperty out{};
        using jvk::QueueFamilyProperties;


        if(contains(properties, QueueFamilyProperties::GRAPHICS)) out |= QueueProperty::GRAPHICS;
        if(contains(properties, QueueFamilyProperties::TRANSFER)) out |= QueueProperty::TRANSFER;
        if(contains(properties, QueueFamilyProperties::COMPUTE)) out |= QueueProperty::COMPUTE;
        if(contains(properties, QueueFamilyProperties::PRESENT)) out |= QueueProperty::PRESENT;

        return out;
    }
}


Queue::Queue(QueueProperty properties) :
    Queue{std::make_unique<jvk::Queue>(to_queue_family_properties(properties))} {}

Queue::Queue(std::unique_ptr<jvk::Queue> vk_queue) : _handle{std::move(vk_queue)} {}

Queue::Queue(QueueProperty properties, State const& state) : Queue{properties} {
    _handle->wrap(state);
}

Queue::~Queue() = default;

void Queue::wrap(State const& state) { _handle->wrap(state); }

void Queue::destroy() { _handle->destroy(); }

void Queue::submit(SubmitInfo& info) const { _handle->const_submit(info.next().raw()); }

void Queue::skip(SubmitInfo& info) const { _handle->const_skip(info.next().raw()); }

void Queue::wait() const { _handle->wait(); }

bool Queue::created() const { return _handle->created(); }

uint32_t Queue::index() const { return _handle->index(); }

uint32_t Queue::familyIndex() const { return _handle->familyIndex(); }

QueueProperty Queue::familyProperties() const { return to_queue_properties(_handle->familyProperties()); }

Queue::Queue(Queue&&) noexcept = default;

Queue& Queue::operator=(Queue&&) noexcept = default;

}
