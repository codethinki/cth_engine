#include "jvk/graph/presenter.hpp"

#include "jvk/base/queue.hpp"
#include "jvk/utility/exception.hpp"

#include "cvk/base/core.hpp"
#include "cvk/surface/surface.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/coro/executor.hpp>
#include <cth/coro/scheduler.hpp>

#include <algorithm>
#include <array>
#include <ranges>
#include <span>
#include <utility>


namespace jvk {
namespace {
    /**
     * @return count of the acquired images that allow a blocking acquire:
     *  VUID-vkAcquireNextImageKHR-swapchain-01802
     */
    [[nodiscard]] uint32_t blocking_acquire_limit(cvk::Swapchain const& swapchain) {
        cvk::Swapchain::debug_check(swapchain);
        auto const capabilities = swapchain.surface().capabilities(
            swapchain.core().physicalDevice()
        );
        auto const imageCount = swapchain.imageCount();
        return imageCount - std::min(capabilities.minImageCount, imageCount);
    }

    template<class T>
    [[nodiscard]] constexpr bool holds(auto const& variant) {
        return std::holds_alternative<T>(variant);
    }

    /**
     * @return the queue operations of the present are enqueued: its fence signals
     * @details the rejected presents too: "the set of queue operations is still considered to be
     *  enqueued", see `vkQueuePresentKHR()`
     */
    [[nodiscard]] constexpr bool enqueued(VkResult result) {
        return result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR
            || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_ERROR_SURFACE_LOST_KHR
            || result == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT
            || result == VK_ERROR_PRESENT_TIMING_QUEUE_FULL_EXT;
    }
}

Presenter::Presenter(Queue& presentQueue, cvk::Swapchain& swapchain, uint32_t framesInFlight) :
    _queue{presentQueue},
    _swapchain{swapchain},
    _blockingAcquireLimit{blocking_acquire_limit(swapchain)},
    _reuses(framesInFlight, Free{}),
    _pendingFences(swapchain.imageCount(), false) {
    CTH_CRITICAL(framesInFlight == 0, "framesInFlight must be > 0") {}

    _acquireSemaphores.reserve(framesInFlight);
    for(uint32_t i = 0; i < framesInFlight; ++i)
        _acquireSemaphores.emplace_back(swapchain.core(), cvk::create);
}

Presenter::~Presenter() {
    CTH_CRITICAL(
        _handedBack.value() != _acquires,
        "every image must be handed back after its present, {} of {} are",
        _handedBack.value(),
        _acquires
    ) {}

    auto const imageIndices = _spares | std::views::transform(&Spare::imageIdx)
        | std::ranges::to<std::vector>();
    // a failed release does not stop the destruction
    try { _swapchain->unacquire(imageIndices); }
    catch(cvk::vk_result_exception const& e) {
        JVK_STABLE_ERR(true, "failed to release the spares, got {}", e.vkResult()) {}
    }
}

cth::co::executor_task<std::optional<Presenter::AcquiredImage>> Presenter::acquire() {
    if(auto const error = takeError(); error != nullptr)
        std::rethrow_exception(error);

    for(;;) {
        // 0 -> no wait
        uint64_t fallbackValue = 0;
        uint64_t handBackValue = 0;
        {
            std::scoped_lock const lock{_mutex};
            if(_outOfDate)
                co_return std::nullopt;
            if(!_spares.empty()) {
                auto const spare = std::move(_spares.back());
                _spares.pop_back();
                co_return handOut(spare.imageIdx, spare.wait);
            }

            if(_fallback)
                fallbackValue = _fallbacks;
            else if(std::ranges::all_of(_reuses, holds<Held, reuse_t>))
                handBackValue = _handBacks.value() + 1;
        }

        if(fallbackValue != 0) {
            co_await _fallbacksDone.wait(fallbackValue);
            continue;
        }
        if(handBackValue != 0) {
            co_await _handBacks.wait(handBackValue);
            continue;
        }

        auto tried = co_await tryInStrand();
        if(auto* const imagePtr = std::get_if<AcquiredImage>(&tried))
            co_return std::move(*imagePtr);

        if(auto const* const readyPtr = std::get_if<SemaphoreSubmitInfo>(&tried))
            co_await readyPtr->semaphore->wait(readyPtr->value);
        else if(auto const* const fallbackPtr = std::get_if<Fallback>(&tried)) {
            auto imageOpt = co_await fallback(*fallbackPtr);
            if(imageOpt.has_value())
                co_return std::move(imageOpt);
        }
    }
}

cth::co::executor_task<void> Presenter::present(AcquiredImage image) {
    CTH_CRITICAL(image._presenter.get() != this, "the image must be of this presenter") {}
    auto const [imageIdx, acquireNumber, wait] = image._presenter.get_deleter();
    // the present hands the image back
    static_cast<void>(image._presenter.release());

    // the images acquired before first: the presents follow the acquire order
    co_await _handedBack.wait(acquireNumber - 1);

    bool fencePending = false;
    for(;;) {
        uint64_t fallbackValue = 0;
        {
            std::scoped_lock const lock{_mutex};
            if(_fallback)
                fallbackValue = _fallbacks;
            else {
                ++_presentsStarted;
                fencePending = _pendingFences[imageIdx];
            }
        }
        if(fallbackValue == 0)
            break;
        co_await _fallbacksDone.wait(fallbackValue);
    }

    VkResult result = VK_ERROR_UNKNOWN;
    std::exception_ptr error = nullptr;
    try {
        // the present resets the fence: the last present of the image signals it first
        if(fencePending)
            waitPresents(std::array{imageIdx});
        result = co_await _queue->present(*_swapchain, imageIdx);
    }
    catch(cvk::vk_result_exception const& e) {
        result = e.vkResult();
        error = std::current_exception();
    }
    catch(...) { error = std::current_exception(); }

    std::scoped_lock const lock{_mutex};
    if(enqueued(result)) {
        // the present ends the acquire, its fence signals once the presentation engine has the
        // references
        _pendingFences[imageIdx] = true;
        --_acquiredImages;
        if(auto const* const semaphorePtr =
            std::get_if<cth::ref_wrapper<cvk::Semaphore const>>(&wait))
            _reuses[semaphoreIdx(**semaphorePtr)] = Presented{imageIdx};
    } else
        // not enqueued: the image stays acquired, the acquire semaphore stays held, no end of the
        // gpu work of its submit is known
        ++_unenqueuedPresents;

    if(error != nullptr) {
        if(_error == nullptr)
            _error = error;
    } else if(result == VK_ERROR_OUT_OF_DATE_KHR)
        _outOfDate = true;

    handBack(acquireNumber);
    _presentsDone.set(_presentsDone.value() + 1);
}

void Presenter::drop(AcquiredImage image) {
    CTH_CRITICAL(image._presenter.get() != this, "the image must be of this presenter") {}
    // the dropper spares the image
    image._presenter.reset();
}

void Presenter::drop(AcquiredImage image, SemaphoreSubmitInfo ready) {
    CTH_CRITICAL(image._presenter.get() != this, "the image must be of this presenter") {}
    auto const [imageIdx, acquireNumber, wait] = image._presenter.get_deleter();
    static_cast<void>(image._presenter.release());

    spare(imageIdx, acquireNumber, wait, ready);
}

bool Presenter::finished() const {
    std::vector<SemaphoreSubmitInfo> readyPoints{};
    std::vector<uint32_t> pendingIndices{};
    {
        std::scoped_lock const lock{_mutex};
        // a present hands back after vkQueuePresentKHR()
        if(_handedBack.value() != _acquires || _unenqueuedPresents != 0)
            return false;

        for(uint32_t imageIdx = 0; imageIdx < _pendingFences.size(); ++imageIdx)
            if(_pendingFences[imageIdx])
                pendingIndices.push_back(imageIdx);

        for(auto const& reuse : _reuses)
            if(auto const* const readyPtr = std::get_if<SemaphoreSubmitInfo>(&reuse))
                readyPoints.push_back(*readyPtr);
        for(auto const& spare : _spares)
            if(auto const* const readyPtr = std::get_if<SemaphoreSubmitInfo>(&spare.wait))
                readyPoints.push_back(*readyPtr);
    }

    // no vulkan call behind the mutex
    return std::ranges::all_of(
            readyPoints,
            [](SemaphoreSubmitInfo const& ready) {
                return ready.semaphore->gpuValue() >= ready.value;
            }
        )
        && std::ranges::all_of(
            pendingIndices,
            [this](uint32_t imageIdx) { return _swapchain->presentFence(imageIdx).signaled(); }
        );
}

void Presenter::waitPresents() { waitPresents(pendingFences()); }

cth::co::executor_task<Presenter::try_t> Presenter::tryInStrand() {
    // the strand serializes the tries and the presents of the presenter, a try starts the fallback
    co_await _queue->executor().schedule();

    {
        std::scoped_lock const lock{_mutex};
        // changed since the check before the strand
        if(_fallback || _outOfDate || !_spares.empty())
            co_return Retry{};
    }

    auto const semaphoreIdxOpt = recycle();
    if(!semaphoreIdxOpt.has_value()) {
        std::scoped_lock const lock{_mutex};
        // a present fence signals soon after the gpu work, a ready point can be far
        if(std::ranges::any_of(_reuses, holds<Presented, reuse_t>))
            co_return startFallback(std::nullopt);

        auto const readyIt = std::ranges::find_if(_reuses, holds<SemaphoreSubmitInfo, reuse_t>);
        if(readyIt != _reuses.end())
            co_return std::get<SemaphoreSubmitInfo>(*readyIt);
        // every semaphore is held again
        co_return Retry{};
    }

    // without the lock: the strand serializes the swapchain, only the tries and the fallback take
    // a free semaphore
    auto const& semaphore = _acquireSemaphores[*semaphoreIdxOpt];
    auto const acquired = _swapchain->acquire(semaphore, 0);

    std::scoped_lock const lock{_mutex};
    if(acquired.imageIndex.has_value()) {
        _reuses[*semaphoreIdxOpt] = Held{};
        ++_acquiredImages;
        co_return handOut(*acquired.imageIndex, cth::ref_wrapper{semaphore});
    }
    if(acquired.result == VK_ERROR_OUT_OF_DATE_KHR) {
        _outOfDate = true;
        co_return Retry{};
    }

    // VK_NOT_READY
    co_return startFallback(semaphoreIdxOpt);
}

cth::co::executor_task<std::optional<Presenter::AcquiredImage>> Presenter::fallback(
    Fallback start
) {
    // no present of the presenter is pending: the blocking acquire waits for no present that the
    // fallback holds back
    co_await _presentsDone.wait(start.presents);
    // a driver thread: the blocking calls stay out of the strand
    co_await cth::co::executor{_queue->scheduler()}.schedule();

    auto semaphoreIdxOpt = start.semaphoreIdxOpt;
    // std::nullopt -> a spare, the retry takes it
    std::optional<cvk::Swapchain::Acquired> acquiredOpt = std::nullopt;
    std::exception_ptr error = nullptr;
    try {
        if(!semaphoreIdxOpt.has_value())
            semaphoreIdxOpt = awaitPresentFence();

        bool spare = false;
        bool blocking = false;
        {
            std::scoped_lock const lock{_mutex};
            spare = !_spares.empty();
            // more acquired images: the presentation engine may never give one, only the try:
            // VUID-vkAcquireNextImageKHR-swapchain-01802
            blocking = _acquiredImages <= _blockingAcquireLimit;
        }

        if(!spare && blocking)
            acquiredOpt = _swapchain->acquire(_acquireSemaphores[*semaphoreIdxOpt]);
        else if(!spare) {
            // the presents before reach the presentation engine first, it can give their images
            // back: no present resets a fence during the fallback
            waitPresents(pendingFences());
            acquiredOpt = _swapchain->acquire(_acquireSemaphores[*semaphoreIdxOpt], 0);
        }
    }
    catch(...) { error = std::current_exception(); }

    uint64_t handBackValue = 0;
    {
        std::scoped_lock const lock{_mutex};
        endFallback();
        if(error != nullptr)
            std::rethrow_exception(error);

        if(acquiredOpt.has_value() && acquiredOpt->imageIndex.has_value()) {
            _reuses[*semaphoreIdxOpt] = Held{};
            ++_acquiredImages;
            co_return handOut(
                *acquiredOpt->imageIndex,
                cth::ref_wrapper{_acquireSemaphores[*semaphoreIdxOpt]}
            );
        }

        if(acquiredOpt.has_value() && acquiredOpt->result == VK_ERROR_OUT_OF_DATE_KHR)
            _outOfDate = true;
        // VK_NOT_READY of the try: only a hand back gives an image, a spare comes at once
        else if(acquiredOpt.has_value() && _spares.empty())
            handBackValue = _handBacks.value() + 1;
    }
    if(handBackValue != 0)
        co_await _handBacks.wait(handBackValue);
    co_return std::nullopt;
}

std::optional<size_t> Presenter::recycle() {
    std::vector<reuse_t> reuses{};
    std::vector<bool> pendingFences{};
    {
        std::scoped_lock const lock{_mutex};
        auto const freeIt = std::ranges::find_if(_reuses, holds<Free, reuse_t>);
        if(freeIt != _reuses.end())
            return static_cast<size_t>(freeIt - _reuses.begin());
        reuses = _reuses;
        pendingFences = _pendingFences;
    }

    // no vulkan call behind the mutex, only the recycle frees a presented or dropped semaphore
    std::vector<uint32_t> signaledIndices{};
    auto const reached = [&](reuse_t const& reuse) {
        if(auto const* const presentedPtr = std::get_if<Presented>(&reuse)) {
            auto const imageIdx = presentedPtr->imageIdx;
            if(!pendingFences[imageIdx])
                return true;
            if(!_swapchain->presentFence(imageIdx).signaled())
                return false;
            signaledIndices.push_back(imageIdx);
            return true;
        }
        if(auto const* const readyPtr = std::get_if<SemaphoreSubmitInfo>(&reuse))
            return readyPtr->semaphore->gpuValue() >= readyPtr->value;
        return false;
    };

    std::optional<size_t> semaphoreIdxOpt = std::nullopt;
    for(size_t i = 0; i < reuses.size(); ++i)
        if(reached(reuses[i])) {
            reuses[i] = Free{};
            semaphoreIdxOpt = semaphoreIdxOpt.value_or(i);
        }
    if(!semaphoreIdxOpt.has_value())
        return std::nullopt;

    std::scoped_lock const lock{_mutex};
    for(size_t i = 0; i < reuses.size(); ++i)
        if(holds<Free>(reuses[i]))
            _reuses[i] = Free{};
    // in the strand or the fallback: no present of the images reset the fences since the reads
    for(auto const imageIdx : signaledIndices)
        _pendingFences[imageIdx] = false;
    return semaphoreIdxOpt;
}

size_t Presenter::awaitPresentFence() {
    for(;;) {
        if(auto const semaphoreIdxOpt = recycle(); semaphoreIdxOpt.has_value())
            return *semaphoreIdxOpt;

        uint32_t imageIdx = 0;
        {
            std::scoped_lock const lock{_mutex};
            auto const presentedIt = std::ranges::find_if(_reuses, holds<Presented, reuse_t>);
            CTH_CRITICAL(
                presentedIt == _reuses.end(),
                "an acquire semaphore must wait for a present fence"
            ) {}
            imageIdx = std::get<Presented>(*presentedIt).imageIdx;
        }
        // the fallback holds the presents back: no reset of the fence during the wait, a fence
        // that is not pending is free at the next recycle
        waitPresents(std::array{imageIdx});
    }
}

std::exception_ptr Presenter::takeError() {
    std::scoped_lock const lock{_mutex};
    return std::exchange(_error, nullptr);
}

std::vector<uint32_t> Presenter::pendingFences() const {
    std::scoped_lock const lock{_mutex};
    std::vector<uint32_t> imageIndices{};
    for(uint32_t imageIdx = 0; imageIdx < _pendingFences.size(); ++imageIdx)
        if(_pendingFences[imageIdx])
            imageIndices.push_back(imageIdx);
    return imageIndices;
}

void Presenter::waitPresents(std::span<uint32_t const> imageIndices) {
    // no vulkan call behind the mutex
    for(auto const imageIdx : imageIndices)
        _swapchain->presentFence(imageIdx).wait();

    std::scoped_lock const lock{_mutex};
    for(auto const imageIdx : imageIndices)
        _pendingFences[imageIdx] = false;
}

auto Presenter::handOut(uint32_t imageIdx, wait_t wait) -> AcquiredImage {
    return AcquiredImage{*this, imageIdx, ++_acquires, std::move(wait)};
}

auto Presenter::startFallback(std::optional<size_t> semaphoreIdxOpt) -> Fallback {
    _fallback = true;
    ++_fallbacks;
    return Fallback{.semaphoreIdxOpt = semaphoreIdxOpt, .presents = _presentsStarted};
}

void Presenter::endFallback() {
    _fallback = false;
    _fallbacksDone.set(_fallbacks);
}

void Presenter::handBack(uint64_t acquireNumber) {
    _handedBackAhead.insert(acquireNumber);
    auto handedBack = _handedBack.value();
    while(_handedBackAhead.erase(handedBack + 1) != 0)
        ++handedBack;
    if(handedBack != _handedBack.value())
        _handedBack.set(handedBack);

    _handBacks.set(_handBacks.value() + 1);
}

void Presenter::spare(
    uint32_t imageIdx,
    uint64_t acquireNumber,
    wait_t const& wait,
    std::optional<SemaphoreSubmitInfo> const& readyOpt
) {
    using semaphore_ref_t = cth::ref_wrapper<cvk::Semaphore const>;

    std::scoped_lock const lock{_mutex};
    // the ready point frees the acquire semaphore once reached, the spare waits for it instead
    if(readyOpt.has_value())
        if(auto const* const semaphorePtr = std::get_if<semaphore_ref_t>(&wait))
            _reuses[semaphoreIdx(**semaphorePtr)] = *readyOpt;

    _spares.push_back(
        Spare{.imageIdx = imageIdx, .wait = readyOpt.has_value() ? wait_t{*readyOpt} : wait}
    );
    handBack(acquireNumber);
}
}
