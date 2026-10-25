#include "jvk/base/queues.hpp"

#include "cvk/base/core.hpp"

#include <cth/data/hash_map.hpp>
#include <cth/io/log.hpp>

#include <algorithm>
#include <cstddef>


namespace jvk {
namespace {
    /**
     * @return number of distinct vulkan queues of the roles
     * @pre @ref cvk::Core::created()
     */
    [[nodiscard]] size_t vulkan_queue_count(cvk::Core const& core) {
        cvk::Core::debug_check(core);

        auto const indices = core.queueIndices();
        return cth::dt::flat_hash_set<size_t>{indices.begin(), indices.end()}.size();
    }
}

Queues::Queues(cvk::Core const& core, size_t workers) :
    _scheduler{cth::co::autostart, workers == 0 ? vulkan_queue_count(core) : workers} {
    cvk::Core::debug_check(core);

    auto const indices = core.queueIndices();
    _queueIndices.reserve(indices.size());

    for(size_t role = 0; role < indices.size(); ++role) {
        auto const first = static_cast<size_t>(std::ranges::find(indices, indices[role]) - indices.begin());
        if(first < role) {
            _queueIndices.push_back(_queueIndices[first]);
            continue;
        }

        _queueIndices.push_back(_queues.size());
        _queues.emplace_back(core.queue(role), _scheduler);
    }
}

Queue const& Queues::queue(size_t role) const {
    CTH_CRITICAL(role >= _queueIndices.size(), "role must be < {}, got {}", _queueIndices.size(), role) {}
    return _queues[_queueIndices[role]];
}
}
