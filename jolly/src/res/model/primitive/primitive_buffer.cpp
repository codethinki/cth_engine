#include "jolly/res/model/primitive/primitive_buffer.hpp"

namespace jly {

PrimitiveBuffer::PrimitiveBuffer(PipelineFlags pipeline_flags, size_t element_size, size_t size)
    : _pipelineFlags{pipeline_flags}, _size{size}, _elementSize{element_size} {}
}
