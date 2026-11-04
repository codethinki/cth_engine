#pragma once
#include "sampler_config.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/handle/destroy.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <tuple>


namespace cvk {
class Core;

/**
 * VkSampler wrapper
 */
class Sampler {
public:
    using Config = SamplerConfig;
    struct State;

    /**
     * base constructor
     * @details clamps @ref Config::maxAnisotropy to maxSamplerAnisotropy of the device
     * @details warns if it clamps
     * @pre @ref Core::created()
     */
    Sampler(Core const&, Config const&);

    /**
     * constructs and wraps
     * @details calls:
     * - @ref Sampler(Core const&, Config const&)
     * - @ref wrap()
     */
    Sampler(Core const&, Config const&, State const&);

    /**
     * constructs and creates
     * @details calls:
     * - @ref Sampler(Core const&, Config const&)
     * - @ref create()
     */
    explicit Sampler(Core const&, Config const&, create_t);

    /**
     * @note calls @ref optDestroy()
     */
    ~Sampler() { optDestroy(); }

    /**
     * wraps the state
     * @pre the sampler was created with the config
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * creates the sampler
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateSampler()
     */
    void create();

    /**
     * destroys and forgets
     * @pre @ref created()
     * @pre the gpu does not use the sampler
     */
    void destroy();

    /**
     * if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * releases ownership, returns the state and forgets
     * @pre @ref created()
     */
    State release();

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VkSampler get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }

    /**
     * @return the config, with the clamped anisotropy
     */
    [[nodiscard]] constexpr Config const& config() const { return _config; }

private:
    void forget();

    core_handle<VkSampler> _handle;
    Config _config;

public:
    Sampler(Sampler const& other) = delete;
    Sampler(Sampler&& other) noexcept = default;
    Sampler& operator=(Sampler const& other) = delete;
    Sampler& operator=(Sampler&& other) noexcept = default;

    static constexpr void debug_check(Sampler const&);
};
}

namespace cvk {
struct Sampler::State {
    vk_not_null<VkSampler> vkSampler;

    /**
     * the raw handles to destroy, in destruction order
     */
    [[nodiscard]] constexpr std::tuple<VkSampler> decompose() && { return {vkSampler.get()}; }
};
}

namespace cvk {
constexpr void Sampler::debug_check(Sampler const& sampler) {
    CTH_CRITICAL(!sampler.created(), "sampler must be created") {}
}
}
