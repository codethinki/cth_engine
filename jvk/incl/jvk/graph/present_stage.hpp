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
 * @details the frame slots own the acquire semaphores, the swapchain has one present semaphore per image
 * @details VK_ERROR_OUT_OF_DATE_KHR and VK_SUBOPTIMAL_KHR flag a recreate
 */
class PresentStage {
public:
    PresentStage(Core const&, PresentStageConfig const&);

    /**
     * acquires the next swapchain image in the strand
     * @details VK_ERROR_OUT_OF_DATE_KHR or extent 0 -> std::nullopt
     * @param signal signaled when the image is ready
     * @return index of the image
     * @pre signal has no pending operation
     * @throws cvk::vk_result_exception other result of vkAcquireNextImageKHR()
     */
    [[nodiscard]] cth::co::executor_task<std::optional<uint32_t>> acquire(cvk::Semaphore const& signal);

    /**
     * posts the present of the image into the strand
     * @details waits for the mark of the acquiring stage off the strand
     * @details returns at once, an error goes to the graph
     * @pre the acquiring stage signals the present semaphore of the image
     */
    void present(uint32_t imageIdx, cth::os::fence const& acquiringStageMark);

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

    [[nodiscard]] constexpr cvk::Swapchain& swapchain() const { return *_swapchainPtr; }
    [[nodiscard]] constexpr Queue const& queue() const { return *_queuePtr; }

private:
    cth::not_null_ptr<Queue const> _queuePtr;
    cth::not_null_ptr<cvk::Swapchain> _swapchainPtr;

    std::atomic<bool> _outOfDate = false;
    std::atomic<bool> _suboptimal = false;

public:
    PresentStage(PresentStage const& other) = delete;
    PresentStage(PresentStage&& other) = delete;
    PresentStage& operator=(PresentStage const& other) = delete;
    PresentStage& operator=(PresentStage&& other) = delete;
};
}
