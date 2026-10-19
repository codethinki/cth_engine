#include "test.hpp"

#include "cvk/utility/constant/device_constants.hpp"
#include "cvk/utility/device/feature_chain.hpp"

#include <string>
#include <vector>


namespace cvk::dev {
namespace {
    using local_read_t = VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR;
    using maintenance1_t = VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR;

    std::vector<std::string> const LOCAL_READ{VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME};
    std::vector<std::string> const BOTH{
        VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME,
    };

    std::vector<VkStructureType> const CORE{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    };

    VkBaseInStructure const* first(FeatureChain const& chain) {
        return reinterpret_cast<VkBaseInStructure const*>(chain.get());
    }

    /**
     * @return the structs of the chain in order, every struct lies in the chain object
     */
    std::vector<VkStructureType> chain_types(FeatureChain const& chain) {
        std::vector<VkStructureType> types{};
        auto const* const begin = reinterpret_cast<std::byte const*>(&chain);
        auto const* const end = begin + sizeof(FeatureChain);

        for(auto const* node = first(chain); node != nullptr; node = node->pNext) {
            auto const* const address = reinterpret_cast<std::byte const*>(node);
            EXPECT_TRUE(address >= begin && address < end) << "the chain points out of its object";
            types.push_back(node->sType);
        }
        return types;
    }

    /**
     * @return the struct of the type in the chain, nullptr -> not in the chain
     */
    template<class T>
    T const* find(FeatureChain const& chain) {
        for(auto const* node = first(chain); node != nullptr; node = node->pNext)
            if(node->sType == feature_struct<T>::TYPE) return reinterpret_cast<T const*>(node);
        return nullptr;
    }
}

CVK_TEST(FeatureChain, without_extensions_links_the_core_structs) {
    FeatureChain const chain{std::vector<std::string>{}};

    EXPECT_EQ(chain_types(chain), CORE);
    EXPECT_TRUE(chain.linked<VkPhysicalDeviceVulkan13Features>());
    EXPECT_FALSE(chain.linked<local_read_t>());
    EXPECT_FALSE(chain.linked<maintenance1_t>());
}

CVK_TEST(FeatureChain, links_the_structs_of_the_extensions) {
    FeatureChain const localRead{LOCAL_READ};
    auto expected = CORE;
    expected.push_back(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES_KHR);
    EXPECT_EQ(chain_types(localRead), expected);

    FeatureChain const both{BOTH};
    expected = CORE;
    expected.push_back(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR);
    expected.push_back(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES_KHR);
    EXPECT_EQ(chain_types(both), expected);
}

CVK_TEST(FeatureChain, copy_and_move_link_their_own_chain) {
    FeatureChain const chain{DeviceFeatures{.multiview = true}, LOCAL_READ};

    FeatureChain const copy{chain};
    EXPECT_EQ(chain_types(copy), chain_types(chain));
    EXPECT_EQ(copy.features(), chain.features());

    FeatureChain moved{FeatureChain{chain}};
    EXPECT_EQ(chain_types(moved), chain_types(chain));

    moved = copy;
    EXPECT_EQ(chain_types(moved), chain_types(chain));
}

CVK_TEST(FeatureChain, bits_go_into_their_vulkan_member) {
    FeatureChain const chain{
        DeviceFeatures{.samplerAnisotropy = true, .synchronization2 = true, .dynamicRenderingLocalRead = true},
        LOCAL_READ
    };

    EXPECT_EQ(chain.get()->features.samplerAnisotropy, VK_TRUE);
    EXPECT_EQ(chain.get()->features.robustBufferAccess, VK_FALSE);

    auto const* const features13 = find<VkPhysicalDeviceVulkan13Features>(chain);
    ASSERT_NE(features13, nullptr);
    EXPECT_EQ(features13->synchronization2, VK_TRUE);
    EXPECT_EQ(features13->dynamicRendering, VK_FALSE);

    auto const* const localRead = find<local_read_t>(chain);
    ASSERT_NE(localRead, nullptr);
    EXPECT_EQ(localRead->dynamicRenderingLocalRead, VK_TRUE);
}

CVK_TEST(FeatureChain, features_round_trip) {
    FeatureChain const chain{constants::BASE_DEVICE_FEATURES, LOCAL_READ};

    EXPECT_EQ(chain.features(), constants::BASE_DEVICE_FEATURES);
}

CVK_TEST(FeatureChain, extensions_of_the_features) {
    static_assert(extensions_of(DeviceFeatures{.synchronization2 = true}).empty());
    static_assert(extensions_of(constants::PRESENT_DEVICE_FEATURES).size() == 1);

    std::vector<std::string_view> const expected{VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME};
    EXPECT_EQ(extensions_of(constants::BASE_DEVICE_FEATURES), expected);
}
}
