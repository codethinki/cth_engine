#pragma once

#include "jolly/res/model/primitive/pipeline_flags.hpp"

namespace jly {
class PrimitiveBuffer {
public:
    PrimitiveBuffer(PipelineFlags pipeline_flags, size_t element_size, size_t size = 0);

private:
    PipelineFlags _pipelineFlags;

    size_t _size;
    size_t _elementSize;
};
}
