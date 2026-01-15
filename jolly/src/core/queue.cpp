#include "jolly/core/queue.hpp"

#include "jolly/core/submit_info.hpp"

namespace jly {

Queue::Queue(Core const& core, jvk::QueueFamilyProperties properties)
    : _core{&core}, _handle{std::make_unique<jvk::Queue>(properties)} {}

Queue::Queue(Core const& core, jvk::QueueFamilyProperties properties, State const& state) : Queue{core, properties} {
    _handle->wrap(state);
}

Queue::~Queue() = default;

void Queue::wrap(State const& state) { _handle->wrap(state); }

void Queue::destroy() { _handle->destroy(); }

void Queue::submit(SubmitInfo& info) const {
    _handle->const_submit(info.next().raw());

}

void Queue::skip(SubmitInfo& info) const { _handle->const_skip(info.next().raw()); }

VkResult Queue::present(uint32_t image_index, jvk::PresentInfo& info) const {
    return _handle->present(image_index, info);
}

void Queue::wait() const { _handle->wait(); }

bool Queue::created() const { return _handle->created(); }

uint32_t Queue::index() const { return _handle->index(); }

uint32_t Queue::familyIndex() const { return _handle->familyIndex(); }

jvk::QueueFamilyProperties Queue::familyProperties() const { return _handle->familyProperties(); }

Queue::Queue(Queue&&) noexcept = default;

Queue& Queue::operator=(Queue&&) noexcept = default;

}
