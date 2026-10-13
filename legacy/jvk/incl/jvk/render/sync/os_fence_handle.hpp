namespace jvk::os {
struct fence_handle_deleter {
    void operator()(void* handle) const;
};


using fence_handle_t = std::unique_ptr<void, fence_handle_deleter>;
}
