#pragma once
#include "queue_properties.hpp"

#include "jolly/utility/types.hpp"


#include <memory>


namespace jvk {
struct PresentInfo;
class Queue;
struct QueueState;
}

namespace jly {
class SubmitInfo;
class Core;
}


namespace jly {

class Queue {
public:
    struct State;

    explicit Queue();
    explicit Queue(std::unique_ptr<jvk::Queue>);


    ~Queue();

    /**
     * @brief wraps the vulkan queue
     */
    void wrap(std::unique_ptr<jvk::Queue>);

    /**
     * @brief destroys and resets the underlying queue
     */
    void destroy();

    /**
     * @brief if @ref created() calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    State release();

    /**
     * @brief advances and submits the submit_info
     */
    void submit(SubmitInfo&) const;

    /**
     * @brief skip submits without the command_buffer to advance the sync primitives
     */
    void skip(SubmitInfo&) const;


    /**
     * Blocks cpu until all queue gpu operations are finished
     */
    void wait() const;


    Queue(Queue const&) = delete;
    Queue& operator=(Queue const&) = delete;
    Queue(Queue&&) noexcept;
    Queue& operator=(Queue&&) noexcept;

private:
    std::unique_ptr<jvk::Queue> _handle;

public:
    [[nodiscard]] jvk::Queue const& raw() const { return *_handle; }
    [[nodiscard]] jvk::Queue const& raw() { return *_handle; }
    [[nodiscard]] bool created() const;
    [[nodiscard]] uint32_t index() const;
    [[nodiscard]] uint32_t familyIndex() const;
    [[nodiscard]] QueueProperties familyProperties() const;
};

}

namespace jly {
struct Queue::State {
    std::unique_ptr<jvk::Queue> handle;
};
}
