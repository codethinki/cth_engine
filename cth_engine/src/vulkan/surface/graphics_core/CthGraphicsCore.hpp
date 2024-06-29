#pragma once
#include "CthBasicGraphicsCore.hpp"

namespace cth {
    class GraphicsCore : public BasicGraphicsCore {
    public:
        explicit GraphicsCore();
        ~GraphicsCore() override = default;

        void create(const std::string_view name, const uint32_t width, const uint32_t height, const Queue* present_queue, const GraphicsSyncConfig& sync_config);
        void destroy();





    };
} //namespace cth