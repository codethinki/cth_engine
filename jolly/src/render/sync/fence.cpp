#include "jolly/render/sync/fence.hpp"

#include "jvk/render/sync/fence.hpp"

#include "jolly/core/core.hpp"

#include "jolly/utility/os/os_asio_include.hpp"
#include "jolly/utility/os/os_def.hpp"

#include <boost/asio/use_awaitable.hpp>

namespace jly {

Fence::Fence(Core const& core) : _core{&core}, _handle{std::make_unique<jvk::Fence>(_core->raw())} {}
Fence::Fence(Core const& core, bool signaled) : Fence{core} { create(signaled); }

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

 Fence::operator co_await() const {
    auto const& executor = co_await boost::asio::this_coro::executor;

    auto nativeHandle = _handle->extractOsHandle();

#ifdef JLY_FS_WINDOWS

    auto const handle = static_cast<HANDLE>(nativeHandle);
    boost::asio::windows::object_handle objHandle(executor, handle);
    co_await objHandle.async_wait(boost::asio::use_awaitable);
#elifdef JLY_FS_POSIX
    //TEMP check if this compiles
    auto fd = static_cast<int>(reinterpret_cast<intptr_t>(nativeHandle));
    boost::asio::posix::stream_descriptor descriptor(executor, fd);
    co_await descriptor.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        boost::asio::use_awaitable
    );
#elif
#error "unsupported file system for co_await"
#endif
}
bool Fence::created() const { return _handle->created(); }

}
