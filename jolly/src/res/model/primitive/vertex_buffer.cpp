#include "jolly/res/model/primitive/vertex_buffer.hpp"

namespace jly {

VertexBuffer::VertexBuffer(jvk::Core const& core, vertex_key key, size_t initial_size)
    : _core{&core},
    _key{std::move(key)},
    _ram{initial_size * _key.vertex_size()},
    _buffer{*_core, _ram.capacity(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT} {}

}
