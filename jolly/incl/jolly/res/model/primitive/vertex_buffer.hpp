#pragma once

#include "vertex_key.hpp"

#include "jvk/base/jvk.hpp"
#include "jvk/res/buffer/base_buffer.hpp"
#include "jolly/utility/types.hpp"

#include <cth/memory/miniram.hpp>

namespace jly {
class VertexBuffer {
public:
    using data_view_t = std::span<std::byte>;

    /**
     * 
     * @param key to identify with
     * @param initial_size 
     */
    VertexBuffer(jvk::Core const& core, vertex_key key, size_t initial_size = 0);

    /**
     * allocs interleaved data
     * @param view to read interleaved data from
     * @note requires view.size() % vertex_key.size() == 0
     */
    void alloc(data_view_t view);

    /**
     * allocs non-interleaved data
     * @param views to read data from
     * @note requires views[n].size() % vertex_key.attributes()[n].size == 0
     */
    void alloc(std::span<data_view_t const> views);

private:
    not_null<jvk::Core const*> _core;

    vertex_key _key;

    cth::mem::miniram _ram;

    jvk::BaseBuffer _buffer;
};
}
