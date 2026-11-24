#include "allocation.hpp"

#include <cstddef>
#include <cstdlib>
#include <new>
#include <utility>


namespace {
// trivially initialized: an allocation can come before the dynamic initialization of a thread
thread_local bool failNext = false;
}

namespace jvk::test {
void fail_next_allocation() noexcept { failNext = true; }
}

// the replaced global operator new, the other forms of new and delete forward to it or match it
void* operator new(std::size_t size) {
    if(std::exchange(failNext, false))
        throw std::bad_alloc{};
    if(void* const ptr = std::malloc(size == 0 ? 1 : size))
        return ptr;
    throw std::bad_alloc{};
}

void operator delete(void* ptr) noexcept { std::free(ptr); }

void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }
