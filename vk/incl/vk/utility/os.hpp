#pragma once



namespace cth::vk {
class Instance;
class Surface;
}

namespace cth::vk::os {
/**
     * @brief creates an operating system specific hidden surface for temporary use
     * @param instance instance to create for
     * @return os specific surface
     */
std::unique_ptr<Surface> TempSurface(Instance const& instance);
}
