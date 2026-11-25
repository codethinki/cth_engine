#pragma once


namespace jvk::test {
/**
 * the next allocation of the calling thread through the global `operator new` throws
 * `std::bad_alloc`
 */
void fail_next_allocation() noexcept;
}
