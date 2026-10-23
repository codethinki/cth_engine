#include "cvk/utility/device/feature_chain.hpp"

#include <algorithm>
#include <ranges>
#include <utility>


namespace cvk::dev {
using std::span;

namespace {
    /**
     * @return the VkBool32 flags of the feature struct
     */
    template<class T>
    [[nodiscard]] auto flags(T& features) {
        using feature_t = std::remove_const_t<T>;
        using bool_t = std::conditional_t<std::is_const_v<T>, VkBool32 const, VkBool32>;
        using byte_t = std::conditional_t<std::is_const_v<T>, std::byte const, std::byte>;

        constexpr size_t count = feature_struct<feature_t>::COUNT;

        auto* const begin = reinterpret_cast<byte_t*>(&features) + feature_struct<feature_t>::OFFSET;
        return std::span<bool_t, count>{reinterpret_cast<bool_t*>(begin), count};
    }

    template<class... Ts>
    [[nodiscard]] feature_structs_t empty_structs(std::type_identity<std::tuple<Ts...>>) {
        return feature_structs_t{Ts{.sType = feature_struct<Ts>::TYPE, .pNext = nullptr}...};
    }
}

FeatureChain::FeatureChain(span<std::string const> extensions) :
    _structs{empty_structs(std::type_identity<feature_structs_t>{})} {
    [&]<size_t... Is>(std::index_sequence<Is...>) {
        ((_linked[Is] = [&] {
            constexpr auto name = feature_struct<std::tuple_element_t<Is, feature_structs_t>>::EXTENSION;
            return name.empty() || std::ranges::contains(extensions, name);
        }()), ...);
    }(std::make_index_sequence<FEATURE_STRUCTS>{});

    link();
}

FeatureChain::FeatureChain(DeviceFeatures const& features, span<std::string const> extensions) :
    FeatureChain{extensions} {
    auto const bits = to_bits(features);

    std::apply(
        [&]<class... Ts>(Ts&... structs) {
            ([&] {
                for(auto const [index, flag] : std::views::enumerate(flags(structs)))
                    flag = bits[feature_offset<Ts> + static_cast<size_t>(index)] ? VK_TRUE : VK_FALSE;
            }(), ...);
        },
        _structs
    );
}

DeviceFeatures FeatureChain::features() const {
    feature_bits_t bits{};

    std::apply(
        [&]<class... Ts>(Ts const&... structs) {
            ([&] {
                for(auto const [index, flag] : std::views::enumerate(flags(structs)))
                    bits[feature_offset<Ts> + static_cast<size_t>(index)] = flag == VK_TRUE;
            }(), ...);
        },
        _structs
    );

    return to_features(bits);
}

void FeatureChain::link() {
    void* next = nullptr;

    // back to front: each linked struct points to the next linked one
    [&]<size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            constexpr size_t index = FEATURE_STRUCTS - 1 - Is;
            auto& features = std::get<index>(_structs);

            features.pNext = _linked[index] ? next : nullptr;
            if(_linked[index]) next = &features;
        }(), ...);
    }(std::make_index_sequence<FEATURE_STRUCTS>{});
}

DeviceFeatures query_features(vk_not_null<VkPhysicalDevice> vkDevice, span<std::string const> extensions) {
    FeatureChain chain{extensions};
    vkGetPhysicalDeviceFeatures2(vkDevice.get(), chain.get());
    return chain.features();
}
}
