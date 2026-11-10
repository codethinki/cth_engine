#include "jvk/base/queues.hpp"

#include "jvk/base/core.hpp"

#include "cvk/base/core.hpp"

#include <cth/data/hash_map.hpp>
#include <cth/io/log.hpp>

#include <algorithm>
#include <cstddef>


namespace jvk {
namespace {
    /**
     * @return number of distinct vulkan queues of the core
     * @pre `cvk::Core::created()`
     */
    [[nodiscard]] size_t vulkan_queue_count(cvk::Core const& core) {
        cvk::Core::debug_check(core);

        auto const indices = core.queueIndices();
        return cth::dt::flat_hash_set<size_t>{indices.begin(), indices.end()}.size();
    }
}

Queues::Queues(Core const& core, size_t workers) :
    _scheduler{cth::co::autostart, workers == 0 ? vulkan_queue_count(core) : workers} {
    cvk::Core const& vk = core;
    cvk::Core::debug_check(vk);

    auto const indices = vk.queueIndices();
    _queueIndices.reserve(indices.size());

    for(size_t queueIdx = 0; queueIdx < indices.size(); ++queueIdx) {
        auto const firstIdx = static_cast<size_t>(std::ranges::find(indices, indices[queueIdx]) - indices.begin());
        if(firstIdx < queueIdx) {
            _queueIndices.push_back(_queueIndices[firstIdx]);
            continue;
        }

        _queueIndices.push_back(_queues.size());
        _queues.emplace_back(core, vk.queue(queueIdx), _scheduler);
    }
}

Queue const& Queues::queue(size_t queueIdx) const {
    CTH_CRITICAL(
        queueIdx >= _queueIndices.size(),
        "queue index must be < {}, got {}",
        _queueIndices.size(),
        queueIdx
    ) {}
    return _queues[_queueIndices[queueIdx]];
}
}
