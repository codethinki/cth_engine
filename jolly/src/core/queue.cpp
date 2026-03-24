#include "jolly/render/submit/queue.hpp"

#include "jolly/render/submit/submit_info.hpp"

namespace jly {

namespace {
    constexpr jvk::QueueFamilyProperties to_queue_family_properties(jly::QueueProperties properties) {
        jvk::QueueFamilyProperties out{};

        if(contains(properties, QueueProperties::COMPUTE)) out |= jvk::QueueFamilyProperties::COMPUTE;
        if(contains(properties, QueueProperties::TRANSFER)) out |= jvk::QueueFamilyProperties::TRANSFER;
        if(contains(properties, QueueProperties::GRAPHICS)) out |= jvk::QueueFamilyProperties::GRAPHICS;
        if(contains(properties, QueueProperties::PRESENT)) out |= jvk::QueueFamilyProperties::PRESENT;

        return out;
    }
}


Queue::Queue(Core const& core, QueueProperties properties)
    : _core{&core}, _handle{std::make_unique<jvk::Queue>(to_queue_family_properties(properties))} {}

Queue::Queue(Core const& core, QueueProperties properties, State const& state) : Queue{core, properties} {
    _handle->wrap(state);
}

Queue::~Queue() = default;

void Queue::wrap(State const& state) { _handle->wrap(state); }

void Queue::destroy() { _handle->destroy(); }

void Queue::submit(SubmitInfo& info) const { _handle->const_submit(info.next().raw()); }

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
