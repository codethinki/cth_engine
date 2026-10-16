#include "cvk/utility/vma_handle.hpp"

#include "cvk/base/allocator.hpp"
#include "cvk/base/core.hpp"


namespace cvk::dev {
VmaAllocator vma_allocator(Core const& core) { return core.allocator().get(); }
}
