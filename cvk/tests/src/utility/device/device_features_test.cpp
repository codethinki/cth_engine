#include "test.hpp"

#include "cvk/utility/constant/device_constants.hpp"
#include "cvk/utility/device/device_features.hpp"

#include <algorithm>
#include <string_view>
#include <vector>


namespace cvk {
CVK_TEST(DeviceFeatures, default_is_false) {
    static_assert(std::ranges::none_of(dev::to_bits(DeviceFeatures{}), std::identity{}));
}

CVK_TEST(DeviceFeatures, bits_follow_the_field_order) {
    static_assert(dev::to_bits(DeviceFeatures{.robustBufferAccess = true})[0]);
    static_assert(dev::to_bits(DeviceFeatures{.dynamicRenderingLocalRead = true})[dev::FEATURE_COUNT - 1]);
    static_assert(dev::to_features(dev::to_bits(constants::BASE_DEVICE_FEATURES)) == constants::BASE_DEVICE_FEATURES);
}

CVK_TEST(DeviceFeatures, merge_enables_the_features_of_both) {
    static_assert([] {
        DeviceFeatures features{.samplerAnisotropy = true};
        features.merge(DeviceFeatures{.synchronization2 = true});

        return features.samplerAnisotropy && features.synchronization2 && !features.dynamicRendering;
    }());
}

CVK_TEST(DeviceFeatures, missing_names_the_features) {
    DeviceFeatures const available{.timelineSemaphore = true};
    DeviceFeatures const required{.samplerAnisotropy = true, .timelineSemaphore = true, .synchronization2 = true};

    std::vector<std::string_view> const expected{"samplerAnisotropy", "synchronization2"};
    EXPECT_EQ(available.missing(required), expected);
}

CVK_TEST(DeviceFeatures, nothing_misses_in_itself) {
    EXPECT_TRUE(constants::BASE_DEVICE_FEATURES.missing(constants::BASE_DEVICE_FEATURES).empty());
}
}
