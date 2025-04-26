export module cth.vk.control;

export namespace cth::vk {

class VkControl {
public:
    VkControl() = delete;

    static void init();
    static void terminate();

private:
    inline static bool _initialized = false;

    static void terminateVolk();
    static void initVolk();

public:
    [[nodiscard]] static bool initialized() { return _initialized; }
};

}
