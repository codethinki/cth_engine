#pragma once
#include "sampler_config.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>
#include <cth/utility.hpp>


namespace cvk {
class Core;

/**
 * @brief VkSampler wrapper
 */
class Sampler {
public:
    using Config = SamplerConfig;
    struct State;

    /**
     * @brief base constructor
     * @details clamps @ref Config::maxAnisotropy to maxSamplerAnisotropy of the device
     * @pre @ref Core::created()
     */
    Sampler(Core const&, Config const&);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref Sampler(Core const&, Config const&)
     * - @ref wrap()
     */
    Sampler(Core const&, Config const&, State const&);

    /**
     * @brief constructs and creates
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
     * @brief wraps the state
     * @pre the sampler was created with the config
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief creates the sampler
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateSampler()
     */
    void create();

    /**
     * @brief destroys and resets
     * @pre @ref created()
     * @pre the gpu does not use the sampler
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership, returns the state and resets
     * @pre @ref created()
     */
    State release();

    static void destroy(DeviceTable table, VkSampler);

    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkSampler get() const { return _handle.get(); }
    [[nodiscard]] Core const& core() const { return *_core; }

    /**
     * @return the config, with the clamped anisotropy
     */
    [[nodiscard]] Config const& config() const { return _config; }

private:
    void reset();

    not_null<Core const*> _core;
    Config _config;
    move_ptr<VkSampler_T> _handle = VK_NULL_HANDLE;

public:
    Sampler(Sampler const& other) = delete;
    Sampler(Sampler&& other) noexcept = default;
    Sampler& operator=(Sampler const& other) = delete;
    Sampler& operator=(Sampler&& other) noexcept { return cth::reconstruct_at(*this, std::move(other)); }

    static void debug_check(Sampler const&);
};
}

namespace cvk {
struct Sampler::State {
    vk_not_null<VkSampler> vkSampler;
};
}

namespace cvk {
inline void Sampler::debug_check(Sampler const& sampler) {
    CTH_CRITICAL(!sampler.created(), "sampler must be created") {}
}
}
