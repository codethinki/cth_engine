#pragma once
#include "jvk/utility/constants.hpp"

#include <cth/macro.hpp>


namespace jly {

class RenderPulse {
    static cxpr size_t DEFAULT_VALUE = 0;

public:
    RenderPulse() = default;
    ~RenderPulse() = default;
    void next() { _subIndex = (get() + 1) % jvk::constants::FRAMES_IN_FLIGHT; }
    void reset() { _subIndex = DEFAULT_VALUE; }

private:
    explicit RenderPulse(size_t val) : _subIndex{val} {}

    std::atomic<size_t> _subIndex = DEFAULT_VALUE;

public:
    [[nodiscard]] auto get() const {
        auto const val = _subIndex.load();
        return val;
    }

    RenderPulse(RenderPulse const& other) = delete;
    RenderPulse& operator=(RenderPulse const& other) = delete;
    RenderPulse(RenderPulse&& other) noexcept : RenderPulse{other.get()} {}

    RenderPulse& operator=(RenderPulse&& other) noexcept {
        _subIndex = other._subIndex.load();
        return *this;
    }
};

}
