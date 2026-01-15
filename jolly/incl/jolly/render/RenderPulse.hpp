#pragma once
#include <cth/macro.hpp>


namespace jly {

class RenderPulse {
    static cxpr size_t DEFAULT_VALUE = 0;

public:
    RenderPulse(size_t frames_in_flight) : RenderPulse{frames_in_flight, DEFAULT_VALUE} {};
    ~RenderPulse() = default;
    void next() { _subIndex = (get() + 1) % _framesInFlight; }
    void reset() { _subIndex = DEFAULT_VALUE; }

private:
    explicit RenderPulse(size_t frames_in_flight, size_t val) : _framesInFlight{frames_in_flight}, _subIndex{val} {}

    size_t _framesInFlight;

    std::atomic<size_t> _subIndex = DEFAULT_VALUE;

public:
    [[nodiscard]] auto framesInFlight() const { return _framesInFlight; }

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
