#include "cvk/utility/device/device_features.hpp"

#include <boost/pfr/core_name.hpp>


namespace cvk {
std::vector<std::string_view> DeviceFeatures::missing(DeviceFeatures const& required) const {
    //IMPLEMENT C++26 reflection gives the names, boost-pfr goes
    static constexpr auto NAMES = boost::pfr::names_as_array<DeviceFeatures>();

    auto const available = dev::to_bits(*this);
    auto const needed = dev::to_bits(required);

    std::vector<std::string_view> names{};
    for(size_t i = 0; i < dev::FEATURE_COUNT; ++i)
        if(needed[i] && !available[i]) names.push_back(NAMES[i]);

    return names;
}
}
