#pragma once
#include "stage_graph_config.hpp"

#include "cvk/render/sync/semaphore.hpp"

#include <volk.h>

#include <cth/coro/tasks/executor_task.hpp>
#include <cth/os/fence.hpp>
#include <cth/ptr/not_null.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>


namespace cvk {
class Swapchain;
}

namespace jvk {
class Core;
class Queue;
}

//TODO review internal: no user present()
namespace jvk::dev {
/**
 * present stage of a @ref StageGraph
 * @details acquire and present run in the strand of the present queue
 * @details one acquire semaphore per frame slot, the swapchain has one present semaphore per image
 * @details VK_ERROR_OUT_OF_DATE_KHR and VK_SUBOPTIMAL_KHR flag a recreate
 */
class PresentStage {
public:
    /**
     * creates the acquire semaphores
     * @throws cvk::vk_result_exception result of vkCreateSemaphore()
     */
    PresentStage(Core const&, PresentStageConfig const&, size_t frameSlots);

    /**
     * acquires the next swapchain image in the strand
     * @details VK_ERROR_OUT_OF_DATE_KHR or extent 0 -> std::nullopt
     * @return index of the image
     * @pre no acquire of the slot is pending
     * @throws cvk::vk_result_exception other result of vkAcquireNextImageKHR()
     */
    [[nodiscard]] cth::co::executor_task<std::optional<uint32_t>> acquire(size_t slot);

    /**
     * posts the present of the image into the strand
     * @details waits for the mark of the image stage off the strand
     * @details returns at once, an error goes to the graph
     * @pre the image stage signals the present semaphore of the image
     */
    void present(uint32_t imageIndex, cth::os::fence const& imageStageMark);

    /**
     * recreates the swapchain in the strand
     * @return the extent changed
     * @pre no acquire or present is pending
     * @throws cvk::vk_result_exception result of vkCreateSwapchainKHR()
     */
    [[nodiscard]] cth::co::executor_task<bool> recreate(VkExtent2D windowExtent);

    /**
     * @return a present or acquire returned VK_ERROR_OUT_OF_DATE_KHR or VK_SUBOPTIMAL_KHR since the last recreate
     */
    [[nodiscard]] bool recreateNeeded() const { return _outOfDate.load() || _suboptimal.load(); }

    [[nodiscard]] cvk::Semaphore const& acquireSemaphore(size_t slot) const { return _acquireSemaphores[slot]; }
    [[nodiscard]] constexpr cvk::Swapchain& swapchain() const { return *_swapchain; }
    [[nodiscard]] constexpr Queue const& queue() const { return *_queue; }

private:
    cth::not_null_ptr<Queue const> _queue;
    cth::not_null_ptr<cvk::Swapchain> _swapchain;
    std::vector<cvk::Semaphore> _acquireSemaphores{};

    std::atomic<bool> _outOfDate = false;
    std::atomic<bool> _suboptimal = false;

public:
    PresentStage(PresentStage const& other) = delete;
    PresentStage(PresentStage&& other) = delete;
    PresentStage& operator=(PresentStage const& other) = delete;
    PresentStage& operator=(PresentStage&& other) = delete;
};
}
