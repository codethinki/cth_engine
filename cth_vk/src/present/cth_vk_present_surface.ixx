export module cth.vk.present.surface;


import cth.vk.res.destruction_queue;
import cth.vk.base.instance;
import cth.vk.base.physical_device;
import cth.vk.util.types;

export namespace cth::vk {
class Surface {
public:
    struct State;

    /**
     * @brief base constructor
     * @param instance must be created
     * @param destruction_queue nullptr or created
     */
    Surface(Instance const& instance, DestructionQueue* destruction_queue) :
        _instance{&instance}, _destructionQueue{destruction_queue} {}

    Surface(Instance const& instance, DestructionQueue* destruction_queue, State const& state);

    ~Surface();

    /**
     * @brief wraps the @ref State
     * @note calls @ref optDestroy()
     */
    void wrap(State const& state);

    /**
     * @brief destroys and resets
     * @note if @ref DestructionQueue is set, pushes to queue
     * @note calls @ref destroy(VkInstance, VkSurfaceKHR)
     * @note requires @ref created()
     */
    void destroy();

    /**
     * @brief calls @ref destroy() if @ref created()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership and resets
     * @note requires @ref created()
     */
    State release();

    [[nodiscard]] bool supportsFamily(PhysicalDevice const& physical_device, uint32_t family_index) const;
    [[nodiscard]] std::vector<VkPresentModeKHR> presentModes(PhysicalDevice const& physical_device) const;
    [[nodiscard]] std::vector<VkSurfaceFormatKHR> formats(PhysicalDevice const& physical_device) const;
    [[nodiscard]] VkSurfaceCapabilitiesKHR capabilities(PhysicalDevice const& physical_device) const;


    /**
     * @brief creates an invisible surface
     * @note should only be used in temp context
     */
    static Surface Temp(Instance const& instance, DestructionQueue* destruction_queue = nullptr);


    /**
     * @brief destroys the @ref VkSurfaceKHR
     */
    static void destroy(vk::not_null<VkInstance> instance, VkSurfaceKHR surface);

private:
    void reset();

    cth::not_null<Instance const*> _instance;
    DestructionQueue* _destructionQueue;

    move_ptr<VkSurfaceKHR_T> _handle = VK_NULL_HANDLE;

public:
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkSurfaceKHR get() const { return _handle.get(); }

    Surface(Surface const& other) = default;
    Surface(Surface&& other) noexcept = delete;
    Surface& operator=(Surface const& other) = default;
    Surface& operator=(Surface&& other) noexcept = delete;

    static void debug_check(Surface const& surface);
    static void debug_check_handle(vk::not_null<VkSurfaceKHR> surface);
};
}

//State

export namespace cth::vk {
struct Surface::State {
    vk::not_null<VkSurfaceKHR> vkSurface;
};

}

//debug check

export namespace cth::vk {
inline void Surface::debug_check(Surface const& surface) { debug_check_handle(surface.get()); }
inline void Surface::debug_check_handle([[maybe_unused]] vk::not_null<VkSurfaceKHR> surface) {}
}
