#pragma once
#include "queue_properties.hpp"

#include "jolly/utility/types.hpp"

#include "jvk/base/queue/queue.hpp"

#include <memory>


namespace jvk {
struct PresentInfo;
}

namespace jly {
class SubmitInfo;
class Core;
}


namespace jly {

class Queue {
public:
    using State = jvk::Queue::State;

    explicit Queue(Core const&, QueueProperties);
    Queue(Core const&, QueueProperties, State const&);

    ~Queue();

    /**
     * @brief wraps the vulkan queue
     */
    void wrap(State const&);

    /**
     * @brief destroys and resets the underlying queue
     */
    void destroy();

    /**
     * @brief if @ref created() calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief advances and submits the submit_info
     */
    void submit(SubmitInfo&) const;

    /**
     * @brief skip submits without the command_buffer to advance the sync primitives
     */
    void skip(SubmitInfo&) const;

    /**
     * @brief presents the image
     * @return result of vkQueuePresentKHR()
     */
    [[nodiscard]] VkResult present(uint32_t image_index, jvk::PresentInfo&) const;

    /**
     * Blocks cpu until all queue gpu operations are finished
     */
    void wait() const;


    Queue(Queue const&) = delete;
    Queue& operator=(Queue const&) = delete;
    Queue(Queue&&) noexcept;
    Queue& operator=(Queue&&) noexcept;

private:
    not_null<Core const*> _core;
    std::unique_ptr<jvk::Queue> _handle;

public:
    [[nodiscard]] jvk::Queue const& raw() const { return *_handle; }
    [[nodiscard]] bool created() const;
    [[nodiscard]] uint32_t index() const;
    [[nodiscard]] uint32_t familyIndex() const;
    [[nodiscard]] jvk::QueueFamilyProperties familyProperties() const;
};

}
