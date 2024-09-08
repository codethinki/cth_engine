#include "PhysicalDeviceFeatures.hpp"

// ReSharper disable once CppUnusedIncludeDirective
#include "../cth_vk_format.hpp"

#include "vulkan/base/CthPhysicalDevice.hpp"

#include<gsl/pointers>


namespace cth::vk::utils {

PhysicalDeviceFeatures::PhysicalDeviceFeatures(VkPhysicalDeviceFeatures2 const& features) : _features(copy2(features)) {}
PhysicalDeviceFeatures::PhysicalDeviceFeatures(vk::not_null<VkPhysicalDevice> vk_device, PhysicalDeviceFeatures const& other) :
    PhysicalDeviceFeatures(other.features()) {
    _free = false; // NOLINT(cppcoreguidelines-prefer-member-initializer) cannot init in delegating constructor (bug in clang-tidy)
    vkGetPhysicalDeviceFeatures2(vk_device.get(), _features.get());

    //auto feature = *reinterpret_cast<VkPhysicalDeviceTimelineSemaphoreFeaturesKHR*>(_features.pNext);
}
PhysicalDeviceFeatures::PhysicalDeviceFeatures(VkPhysicalDeviceFeatures2 const& a, VkPhysicalDeviceFeatures2 const& b) : PhysicalDeviceFeatures(a) {
    merge2(&b);
}
PhysicalDeviceFeatures::~PhysicalDeviceFeatures() { destroy(); }

auto PhysicalDeviceFeatures::supports(PhysicalDeviceFeatures const& required_features) const -> std::vector<std::variant<size_t, VkStructureType>> {
    DEBUG_CHECK_PHYSICAL_DEVICE_FEATURES(this);
    return support(*_features, required_features.features());
}



auto PhysicalDeviceFeatures::support(VkPhysicalDeviceFeatures2 const& available_features,
    VkPhysicalDeviceFeatures2 const& required_features) -> std::vector<std::variant<size_t, VkStructureType>> {
    std::vector<std::variant<size_t, VkStructureType>> missingFeatures{};

    auto const result = support(available_features.features, required_features.features);
    missingFeatures.insert(missingFeatures.end(), result.begin(), result.end());

    auto const* required = static_cast<VkBaseOutStructure*>(required_features.pNext);
    while(required != nullptr) {
        auto const available = find2(&available_features, required->sType);
        if(available == nullptr || !checkSupport2(available, required)) missingFeatures.emplace_back(required->sType);

        required = required->pNext;
    }

    return missingFeatures;
}


auto PhysicalDeviceFeatures::support(VkPhysicalDeviceFeatures const& available_features,
    VkPhysicalDeviceFeatures const& required_features) -> std::vector<size_t> {

    constexpr size_t features = sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);

    auto const availableFlags = to_span(available_features);
    auto const requiredFlags = to_span(required_features);

    std::vector<size_t> missingFeatures{};

    for(size_t i = 0; i < features; i++)
        if(requiredFlags[i] && !availableFlags[i]) missingFeatures.push_back(i);

    return missingFeatures;
}
void PhysicalDeviceFeatures::merge(PhysicalDeviceFeatures const& other) {
    DEBUG_CHECK_PHYSICAL_DEVICE_FEATURES(this);


    if(other.empty()) return;
    merge2(other._features.get());
}

void PhysicalDeviceFeatures::destroy() {
    if(_free && _features != nullptr) {
        auto ptr = static_cast<gsl::owner<VkBaseOutStructure*>>(_features->pNext);
        while(ptr != nullptr) {
            // ReSharper disable once CppRedundantCastExpression
            auto const next = static_cast<gsl::owner<VkBaseOutStructure*>>(ptr->pNext);
            std::free(ptr); // NOLINT(cppcoreguidelines-no-malloc)
            ptr = next;
        }
    }

    _features = nullptr;
}

void PhysicalDeviceFeatures::merge(VkPhysicalDeviceFeatures const& features) {
    DEBUG_CHECK_PHYSICAL_DEVICE_FEATURES(this);

    auto const aFlags = to_span(_features->features);
    auto const bFlags = to_span(features);

    for(auto [aFlag, bFlag] : std::views::zip(aFlags, bFlags))
        aFlag |= bFlag;
}


void PhysicalDeviceFeatures::merge2(VkPhysicalDeviceFeatures2 const* features2) {
    merge(features2->features);

    auto feature = static_cast<VkBaseOutStructure*>(features2->pNext);
    while(feature != nullptr) {
        auto const dst = find2(_features.get(), feature->sType);

        if(dst != nullptr) merge2(feature, dst);
        else {
            gsl::owner<VkBaseOutStructure*> const copy = copy2(feature);
            copy->pNext = static_cast<VkBaseOutStructure*>(_features->pNext);
            _features->pNext = copy;
        }

        feature = feature->pNext;
    }
}

bool PhysicalDeviceFeatures::checkSupport2(VkBaseOutStructure const* available_feature2,
    VkBaseOutStructure const* required_feature2) {
    CTH_ERR(available_feature2->sType != required_feature2->sType, "feature types must be equal")
        throw details->exception();

    auto const availableFlags = to_bool_args(available_feature2);
    auto const requiredFlags = to_bool_args(required_feature2);

    return std::ranges::equal(availableFlags, requiredFlags, [](VkBool32 available_flag, VkBool32 required_flag) {
        return (available_flag & required_flag) == required_flag;
    });
}

void PhysicalDeviceFeatures::merge2(VkBaseOutStructure const* from, VkBaseOutStructure* to) {
    CTH_ERR(from->sType != to->sType, "merging different types is not allowed, from({0}), to({1})", from->sType, to->sType)
        throw details->exception();

    auto const fromFlags = to_bool_args(from);
    auto const toFlags = to_bool_args(to);

    for(auto [fromFlag, toFlag] : std::views::zip(fromFlags, toFlags))
        toFlag |= fromFlag;
}
auto PhysicalDeviceFeatures::copy2(VkBaseOutStructure const* feature2) -> gsl::owner<VkBaseOutStructure*> {
    size_t const flagBytes = flagCount2(feature2->sType) * sizeof(VkBool32);

    gsl::owner<void*> const memory = std::malloc(sizeof(VkBaseOutStructure) + flagBytes);

    std::memcpy(memory, feature2, sizeof(VkBaseOutStructure) + flagBytes);

    auto const ptr = static_cast<VkBaseOutStructure*>(memory);
    ptr->pNext = nullptr;
    return ptr;
}



auto PhysicalDeviceFeatures::copy2(VkPhysicalDeviceFeatures2 const& features) -> std::unique_ptr<VkPhysicalDeviceFeatures2> {
    auto copy = std::make_unique<VkPhysicalDeviceFeatures2>(features);
    auto last = reinterpret_cast<VkBaseOutStructure*>(copy.get());
    VkBaseOutStructure const* ptr = static_cast<VkBaseOutStructure*>(features.pNext);
    while(ptr != nullptr) {
        last->pNext = copy2(ptr); // NOLINT(cppcoreguidelines-owning-memory)
        ptr = ptr->pNext;
        last = last->pNext;
    }


    return std::move(copy);
}

auto PhysicalDeviceFeatures::find2(VkPhysicalDeviceFeatures2 const* features, VkStructureType type) -> VkBaseOutStructure* {
    auto* ptr = static_cast<VkBaseOutStructure*>(features->pNext);

    while(ptr != nullptr && ptr->sType != type)
        ptr = ptr->pNext;

    return ptr;
}

size_t PhysicalDeviceFeatures::flagCount2(VkStructureType feature_type) {
    switch(feature_type) {
        //one bool comparison
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR:
            return 1;
        default:
            CTH_ERR(true, "unknown feature structure feature_type: ({})", feature_type) throw details->exception();
    }
}
void PhysicalDeviceFeatures::debug_check(cth::not_null<PhysicalDeviceFeatures const*> features) {
    CTH_ERR(!features->created(), "features must be created") throw details->exception();
}


constexpr auto PhysicalDeviceFeatures::indexToString(size_t index) -> std::string_view {
    switch(index) {
        case 0:
            return "robustBufferAccess";
        case 1:
            return "fullDrawIndexUint32";
        case 2:
            return "imageCubeArray";
        case 3:
            return "independentBlend";
        case 4:
            return "geometryShader";
        case 5:
            return "tessellationShader";
        case 6:
            return "sampleRateShading";
        case 7:
            return "dualSrcBlend";
        case 8:
            return "logicOp";
        case 9:
            return "multiDrawIndirect";
        case 10:
            return "drawIndirectFirstInstance";
        case 11:
            return "depthClamp";
        case 12:
            return "depthBiasClamp";
        case 13:
            return "fillModeNonSolid";
        case 14:
            return "depthBounds";
        case 15:
            return "wideLines";
        case 16:
            return "largePoints";
        case 17:
            return "alphaToOne";
        case 18:
            return "multiViewport";
        case 19:
            return "samplerAnisotropy";
        case 20:
            return "textureCompressionETC2";
        case 21:
            return "textureCompressionASTC_LDR";
        case 22:
            return "textureCompressionBC";
        case 23:
            return "occlusionQueryPrecise";
        case 24:
            return "pipelineStatisticsQuery";
        case 25:
            return "vertexPipelineStoresAndAtomics";
        case 26:
            return "fragmentStoresAndAtomics";
        case 27:
            return "shaderTessellationAndGeometryPointSize";
        case 28:
            return "shaderImageGatherExtended";
        case 29:
            return "shaderStorageImageExtendedFormats";
        case 30:
            return "shaderStorageImageMultisample";
        case 31:
            return "shaderStorageImageReadWithoutFormat";
        case 32:
            return "shaderStorageImageWriteWithoutFormat";
        case 33:
            return "shaderUniformBufferArrayDynamicIndexing";
        case 34:
            return "shaderSampledImageArrayDynamicIndexing";
        case 35:
            return "shaderStorageBufferArrayDynamicIndexing";
        case 36:
            return "shaderStorageImageArrayDynamicIndexing";
        case 37:
            return "shaderClipDistance";
        case 38:
            return "shaderCullDistance";
        case 39:
            return "shaderFloat64";
        case 40:
            return "shaderInt64";
        case 41:
            return "shaderInt16";
        case 42:
            return "shaderResourceResidency";
        case 43:
            return "shaderResourceMinLod";
        case 44:
            return "sparseBinding";
        case 45:
            return "sparseResidencyBuffer";
        case 46:
            return "sparseResidencyImage2D";
        case 47:
            return "sparseResidencyImage3D";
        case 48:
            return "sparseResidency2Samples";
        case 49:
            return "sparseResidency4Samples";
        case 50:
            return "sparseResidency8Samples";
        case 51:
            return "sparseResidency16Samples";
        case 52:
            return "sparseResidencyAliased";
        case 53:
            return "variableMultisampleRate";
        case 54:
            return "inheritedQueries";
        default:
            return "Invalid index";
    }
}
} //namespace cth::vk::utils
