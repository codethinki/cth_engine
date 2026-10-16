#include "cvk/utility/unique_handle.hpp"

#include "cvk/base/instance.hpp"


namespace cvk::dev {
VkInstance vk_instance(Instance const& instance) { return instance.get(); }
}
