#pragma once
#include <gsl/pointers>


namespace cth::vk::utils {

class PhysicalDeviceFeatures {
public:
    PhysicalDeviceFeatures() = default;
    /**
     * @param features copies the features
     */
    explicit PhysicalDeviceFeatures(VkPhysicalDeviceFeatures2 const& features);

    explicit PhysicalDeviceFeatures(VkPhysicalDevice vk_device, PhysicalDeviceFeatures const& other);

    /**
     * @brief copies and merges the features
     */
    explicit PhysicalDeviceFeatures(VkPhysicalDeviceFeatures2 const& a, VkPhysicalDeviceFeatures2 const& b);
    ~PhysicalDeviceFeatures();

    /**
     * @return unsupported features
     */
    [[nodiscard]] auto supports(PhysicalDeviceFeatures const& required_features) const -> std::vector<std::variant<size_t, VkStructureType>>;

    /**
     * @return unsupported features 
     */
    [[nodiscard]] static auto support(VkPhysicalDeviceFeatures2 const& available_features, VkPhysicalDeviceFeatures2 const& required_features)
        -> std::vector<std::variant<size_t, VkStructureType>>;

    /**
    * @return indices of missing features
    */
    [[nodiscard]] static std::vector<size_t> support(VkPhysicalDeviceFeatures const& available_features,
        VkPhysicalDeviceFeatures const& required_features);

    void merge(PhysicalDeviceFeatures const& other);

private:
    void destroy();

    void merge(VkPhysicalDeviceFeatures const& features) const;
    void merge2(VkPhysicalDeviceFeatures2 const* features2);



    /**
     * @brief checks the support based on the feature2 type
     * @return support result
     */
    [[nodiscard]] static bool checkSupport2(VkBaseOutStructure const* available_feature2, VkBaseOutStructure const* required_feature2);

    static void merge2(VkBaseOutStructure const* from, VkBaseOutStructure* to);
    [[nodiscard]] static gsl::owner<VkBaseOutStructure*> copy2(VkBaseOutStructure const* feature2);
    [[nodiscard]] static std::unique_ptr<VkPhysicalDeviceFeatures2> copy2(VkPhysicalDeviceFeatures2 const& features);

    [[nodiscard]] static VkBaseOutStructure* find2(VkPhysicalDeviceFeatures2 const* features, VkStructureType type);

    [[nodiscard]] static size_t flagCount2(VkStructureType feature_type);
    [[nodiscard]] static constexpr std::string_view indexToString(size_t index);
    template<class T> requires(std::same_as<VkPhysicalDeviceFeatures, type::pure_t<T>>)
    [[nodiscard]] static auto to_span(T& features);
    template<class T> requires(std::same_as<VkBaseOutStructure, type::pure_t<T>>)
    [[nodiscard]] static auto to_bool_args(T* feature2);

    std::unique_ptr<VkPhysicalDeviceFeatures2> _features;
    bool _free = true;

public:
    [[nodiscard]] VkPhysicalDeviceFeatures2 features() const { return *_features; }
    [[nodiscard]] VkPhysicalDeviceFeatures2* get() const { return _features.get(); }
    [[nodiscard]] bool empty() const { return _features == nullptr; }

    PhysicalDeviceFeatures(PhysicalDeviceFeatures const& other) : PhysicalDeviceFeatures(other.features()) {}
    PhysicalDeviceFeatures& operator=(PhysicalDeviceFeatures const& other) {
        if(this == &other) return *this;

        this->destroy();
        this->_features = copy2(other.features());

        return *this;
    }
    PhysicalDeviceFeatures(PhysicalDeviceFeatures&& other) noexcept = default;
    PhysicalDeviceFeatures& operator=(PhysicalDeviceFeatures&& other) noexcept = default;
};


template<class T> requires (std::same_as<VkPhysicalDeviceFeatures, type::pure_t<T>>)
auto PhysicalDeviceFeatures::to_span(T& features) {
    using bool_t = std::conditional_t<std::is_const_v<T>, VkBool32 const, VkBool32>;

    constexpr size_t size = sizeof(VkPhysicalDeviceFeatures) / 4;
    return std::span<bool_t, size>{reinterpret_cast<bool_t*>(&features), size};
}

template<class T> requires (std::same_as<VkBaseOutStructure, type::pure_t<T>>)
auto PhysicalDeviceFeatures::to_bool_args(T* feature2) {
    using bool_t = std::conditional_t<std::is_const_v<T>, VkBool32 const, VkBool32>;

    return std::span<bool_t>{reinterpret_cast<bool_t*>(feature2 + 1), PhysicalDeviceFeatures::flagCount2(feature2->sType)};
}



} //namespace cth::vk::utils
