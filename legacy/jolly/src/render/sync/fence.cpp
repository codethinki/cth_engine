#include "jolly/render/sync/fence.hpp"

#include "jvk/render/sync/fence.hpp"

#include "jolly/core/core.hpp"


namespace jly {

Fence::Fence(Core const& core) : _core{&core}, _handle{std::make_unique<jvk::Fence>(_core->raw())} {}
Fence::Fence(Core const& core, bool signaled) : Fence{core} { create(signaled); }
Fence::~Fence() = default;

// ReSharper disable CppMemberFunctionMayBeConst
void Fence::create(bool signaled) {
    auto const flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
    _handle->create(flags);
}
void Fence::destroy() { _handle->destroy(); }
// ReSharper restore CppMemberFunctionMayBeConst

bool Fence::signaled() const { return _handle->signaled(); }
bool Fence::wait(size_t timeout) const { return _handle->wait(timeout) == VK_SUCCESS; }
void Fence::wait() const { _handle->wait(); }


// ReSharper disable CppMemberFunctionMayBeConst
void Fence::reset() { _handle->reset(); }

bool Fence::waitReset(size_t timeout) { return _handle->waitReset(timeout); }
void Fence::waitReset() { _handle->waitReset(); }
// ReSharper restore CppMemberFunctionMayBeConst

cth::co::native_handle_awaiter Fence::operator co_await() const {
    return {_core->scheduler(), _handle->extractOsHandle()};
}
bool Fence::created() const { return _handle->created(); }
Fence::Fence(Fence&& other) noexcept = default;

}
