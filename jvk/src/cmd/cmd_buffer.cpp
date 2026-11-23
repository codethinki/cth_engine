#include "jvk/cmd/cmd_buffer.hpp"

#include "cvk/base/core.hpp"
#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <cth/io/log.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <array>
#include <bit>
#include <optional>


namespace jvk::dev {
namespace {
    template<class Handle>
    [[nodiscard]] constexpr ImageAccess shader_access(
        Handle const& handle,
        VkPipelineStageFlags2 stages,
        VkAccessFlags2 access
    ) {
        return ImageAccess{
            .layout = Handle::LAYOUT,
            .stages = stages,
            .access = access,
            .firstMipLevel = handle.range().baseMipLevel,
            .mipLevels = handle.range().levelCount,
        };
    }

    [[nodiscard]] constexpr bool discards(VkAttachmentLoadOp loadOp) {
        return loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR || loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }

    [[nodiscard]] constexpr bool depth_read_only(VkImageLayout layout) {
        return layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
            || layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            || layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
            || layout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
    }

    [[nodiscard]] constexpr bool stencil_read_only(VkImageLayout layout) {
        return layout == VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL
            || layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            || layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
            || layout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
    }

    [[nodiscard]] ImageAccess view_access(
        cvk::ImageView const& view,
        VkImageLayout layout,
        VkPipelineStageFlags2 stages,
        VkAccessFlags2 access,
        bool discard
    ) {
        return ImageAccess{
            .layout = layout,
            .stages = stages,
            .access = access,
            .firstMipLevel = view.range().baseMipLevel,
            .mipLevels = 1,
            .discard = discard,
        };
    }

    [[nodiscard]] ImageAccess depth_stencil_access(
        cvk::RenderingAttachment const& attachment,
        bool writes
    ) {
        constexpr VkPipelineStageFlags2 tests = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
            | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        auto const access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT
            | (writes ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_2_NONE);
        return view_access(
            attachment.view,
            attachment.layout,
            tests,
            access,
            discards(attachment.loadOp)
        );
    }

    [[nodiscard]] bool same_mip_level(cvk::ImageView const& view, cvk::ImageView const& other) {
        return &view.image() == &other.image()
            && view.range().baseMipLevel == other.range().baseMipLevel;
    }


    // a callable, no vector: no allocation per rendering
    template<class Visit>
    void visit_attachment_accesses(cvk::RenderingInfo const& info, Visit visit) {
        for(auto const& attachment : info.colorAttachments) {
            auto const access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
                | (attachment.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD
                    ? VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT
                    : VK_ACCESS_2_NONE);
            visit(
                attachment.view->image(),
                view_access(
                    attachment.view,
                    attachment.layout,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    access,
                    discards(attachment.loadOp)
                )
            );
        }

        auto const& depth = info.depthAttachment;
        auto const& stencil = info.stencilAttachment;
        bool const shared = depth.has_value() && stencil.has_value()
            && same_mip_level(depth->view, stencil->view);
        if(shared) {
            CTH_CRITICAL(
                depth->layout != stencil->layout,
                "the depth and the stencil attachment of one image must have the same layout, got {} and {}",
                depth->layout,
                stencil->layout
            ) {}

            auto access = depth_stencil_access(*depth, !depth_read_only(depth->layout));
            access.access |= depth_stencil_access(
                *stencil,
                !stencil_read_only(stencil->layout)
            ).access;
            access.discard = discards(depth->loadOp) && discards(stencil->loadOp);
            visit(depth->view->image(), access);
        } else {
            if(depth.has_value())
                visit(
                    depth->view->image(),
                    depth_stencil_access(*depth, !depth_read_only(depth->layout))
                );
            if(stencil.has_value())
                visit(
                    stencil->view->image(),
                    depth_stencil_access(*stencil, !stencil_read_only(stencil->layout))
                );
        }

        auto const resolve = [&](cvk::RenderingAttachment const& attachment) {
            if(attachment.resolveViewPtr == nullptr)
                return;
            cvk::ImageView const& resolveView = *attachment.resolveViewPtr;
            visit(
                resolveView.image(),
                view_access(
                    resolveView,
                    attachment.resolveLayout,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    false
                )
            );
        };
        for(auto const& attachment : info.colorAttachments)
            resolve(attachment);
        if(depth.has_value())
            resolve(*depth);
        if(stencil.has_value() && !(shared && stencil->resolveViewPtr == depth->resolveViewPtr))
            resolve(*stencil);

    }
}

template<class Cvk>
BasicCmdBuffer<Cvk>::BasicCmdBuffer(
    cvk::Core const& core,
    cvk::queue_family_index_t queueFamilyIndex
) :
    BasicCmdBuffer{
        std::make_unique<cvk::CmdPool>(
            core,
            cvk::CmdPool::Config{.queueFamilyIndex = queueFamilyIndex, .flags = POOL_FLAGS}
        )
    } {}

template<class Cvk>
BasicCmdBuffer<Cvk>::BasicCmdBuffer(std::unique_ptr<cvk::CmdPool> poolPtr) :
    _cmdBuffer{*poolPtr},
    _poolPtr{std::move(poolPtr)} {}

template<class Cvk>
void BasicCmdBuffer<Cvk>::wrap(State const& state) {
    optDestroy();

    _poolPtr->wrap(cvk::CmdPool::State{.vkPool = state.vkPool});
    _cmdBuffer.wrap(cvk::BaseCmdBuffer::State{.vkPool = state.vkPool, .vkBuffer = state.vkBuffer});
}

template<class Cvk>
void BasicCmdBuffer<Cvk>::create() {
    optDestroy();

    _poolPtr->create();

    // the command buffer needs the created pool, only a catch destroys the pool when the allocation fails
    try { _cmdBuffer.create(); }
    catch(...) {
        _poolPtr->destroy();
        throw;
    }
}

template<class Cvk>
void BasicCmdBuffer<Cvk>::destroy() {
    Cvk::debug_check(_cmdBuffer);

    _cmdBuffer.destroy();
    _poolPtr->destroy();

    forget();
}

template<class Cvk>
auto BasicCmdBuffer<Cvk>::release() -> State {
    Cvk::debug_check(_cmdBuffer);

    // the command buffer first: the pool releases only without counted command buffers
    auto const cmdBuffer = _cmdBuffer.release();
    auto const pool = _poolPtr->release();

    forget();
    return State{.vkPool = pool.vkPool, .vkBuffer = cmdBuffer.vkBuffer};
}

template<class Cvk>
void BasicCmdBuffer<Cvk>::use(cvk::Image const& image, ImageAccess const& access) {
    cvk::Barrier barrier{};
    _states.access(image, access, barrier);
    record(barrier);
}

template<class Cvk>
void BasicCmdBuffer<Cvk>::use(cvk::BaseBuffer const& buffer, BufferAccess const& access) {
    cvk::Barrier barrier{};
    _states.access(buffer, access, barrier);
    record(barrier);
}

template<class Cvk>
void BasicCmdBuffer<Cvk>::use(SampledImage const& handle, VkPipelineStageFlags2 stages) {
    use(handle.image(), shader_access(handle, stages, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT));
}

template<class Cvk>
void BasicCmdBuffer<
    Cvk>::use(StorageImage const& handle, VkPipelineStageFlags2 stages, VkAccessFlags2 access) {
    use(handle.image(), shader_access(handle, stages, access));
}

template<class Cvk>
void BasicCmdBuffer<Cvk>::use(InputAttachment const& handle) {
    use(
        handle.image(),
        shader_access(
            handle,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT
        )
    );
}

template<class Cvk>
void BasicCmdBuffer<Cvk>::assume(
    cvk::Image const& image,
    ImageState const& state,
    uint32_t firstMipLevel,
    uint32_t mipLevels
) {
    CTH_CRITICAL(!recording(), "command buffer must be recording") {}
    _states.assume(image, state, firstMipLevel, mipLevels);
}

template<class Cvk>
void BasicCmdBuffer<Cvk>::assume(cvk::BaseBuffer const& buffer, BufferState const& state) {
    CTH_CRITICAL(!recording(), "command buffer must be recording") {}
    _states.assume(buffer, state);
}

template<class Cvk>
scoped_rendering<BasicCmdBuffer<Cvk>> BasicCmdBuffer<Cvk>::beginRendering(
    cvk::RenderingInfo const& info
) {
    cvk::Barrier barrier{};
    visit_attachment_accesses(
        info,
        [&](cvk::Image const& image, ImageAccess const& access) {
            _states.access(image, access, barrier);
        }
    );

    record(barrier);
    _cmdBuffer.beginRendering(info);
    return scoped_rendering<BasicCmdBuffer>{*this};
}

template<class Cvk>
void BasicCmdBuffer<Cvk>::endRendering() {
    CTH_CRITICAL(rendering() == nullptr, "command buffer must be inside its own rendering") {}

    visit_attachment_accesses(
        *rendering(),
        [this](cvk::Image const& image, ImageAccess const& access) {
            _states.extend(
                image,
                cvk::BarrierScope{.stages = access.stages, .access = access.access},
                access.firstMipLevel,
                access.mipLevels
            );
        }
    );

    _cmdBuffer.endRendering();
}

template<class Cvk>
void BasicCmdBuffer<Cvk>::bindVertices(
    uint32_t binding,
    cvk::BaseBuffer const& buffer,
    size_t offset
) {
    use(
        buffer,
        {VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT}
    );
    _cmdBuffer.bindVertices(binding, buffer, offset);
}

template<class Cvk>
void BasicCmdBuffer<Cvk>::bindIndices(
    cvk::BaseBuffer const& buffer,
    VkIndexType type,
    size_t offset
) {
    use(buffer, {VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, VK_ACCESS_2_INDEX_READ_BIT});
    _cmdBuffer.bindIndices(buffer, type, offset);
}

template<class Cvk>
void BasicCmdBuffer<Cvk>::drawIndirect(
    cvk::BaseBuffer const& buffer,
    size_t offset,
    uint32_t drawCount,
    uint32_t stride
) {
    use(buffer, {VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});
    _cmdBuffer.drawIndirect(buffer, offset, drawCount, stride);
}

template<class Cvk>
void BasicCmdBuffer<Cvk>::drawIndexedIndirect(
    cvk::BaseBuffer const& buffer,
    size_t offset,
    uint32_t drawCount,
    uint32_t stride
) {
    use(buffer, {VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});
    _cmdBuffer.drawIndexedIndirect(buffer, offset, drawCount, stride);
}

template<class Cvk>
void BasicCmdBuffer<Cvk>::dispatchIndirect(cvk::BaseBuffer const& buffer, size_t offset) {
    use(buffer, {VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT});
    _cmdBuffer.dispatchIndirect(buffer, offset);
}

template<class Cvk>
void BasicCmdBuffer<Cvk>::reset() {
    _poolPtr->reset();
    _states.clear();
}

template<class Cvk>
void BasicCmdBuffer<Cvk>::appendStates(ResourceTracker const& next) {
    cvk::Barrier barrier{};
    _states.append(next, barrier);
    record(barrier);
}

template<class Cvk>
void BasicCmdBuffer<Cvk>::record(cvk::Barrier& barrier) const {
    if(barrier.empty())
        return;

    if(insideRendering()) {
        barrier.flags |= VK_DEPENDENCY_BY_REGION_BIT;
        if(std::popcount(renderingViewMask()) > 1)
            barrier.flags |= VK_DEPENDENCY_VIEW_LOCAL_BIT;
    }
    barrier.execute(_cmdBuffer);
}

template<class Cvk>
void BasicCmdBuffer<Cvk>::forget() { _states.clear(); }

template class BasicCmdBuffer<cvk::PrimaryCmdBuffer>;
template class BasicCmdBuffer<cvk::SecondaryCmdBuffer>;
}


namespace jvk {
PrimaryCmdBuffer::PrimaryCmdBuffer(
    cvk::Core const& core,
    cvk::queue_family_index_t queueFamilyIndex
) :
    BasicCmdBuffer{core, queueFamilyIndex} {}

PrimaryCmdBuffer::PrimaryCmdBuffer(
    cvk::Core const& core,
    cvk::queue_family_index_t queueFamilyIndex,
    State const& state
) :
    PrimaryCmdBuffer{core, queueFamilyIndex} { wrap(state); }

PrimaryCmdBuffer::PrimaryCmdBuffer(
    cvk::Core const& core,
    cvk::queue_family_index_t queueFamilyIndex,
    cvk::create_t
) :
    PrimaryCmdBuffer{core, queueFamilyIndex} { create(); }

void PrimaryCmdBuffer::begin(VkCommandBufferUsageFlags usage) {
    reset();
    _cmdBuffer.begin(usage);
}

void PrimaryCmdBuffer::execute(std::span<cth::ref_wrapper<SecondaryCmdBuffer> const> secondaries) {
    for(SecondaryCmdBuffer& secondary : secondaries)
        secondary.optEnd();

    for(SecondaryCmdBuffer const& secondary : secondaries) {
        appendStates(secondary.states());
        _cmdBuffer.execute(secondary);
    }
}

void PrimaryCmdBuffer::execute(
    std::initializer_list<cth::ref_wrapper<SecondaryCmdBuffer>> secondaries
) { execute(std::span{secondaries}); }

void PrimaryCmdBuffer::execute(SecondaryCmdBuffer& secondary) {
    std::array const secondaries{cth::ref_wrapper{secondary}};
    execute(secondaries);
}

SecondaryCmdBuffer::SecondaryCmdBuffer(
    cvk::Core const& core,
    cvk::queue_family_index_t queueFamilyIndex
) :
    BasicCmdBuffer{core, queueFamilyIndex} {}

SecondaryCmdBuffer::SecondaryCmdBuffer(
    cvk::Core const& core,
    cvk::queue_family_index_t queueFamilyIndex,
    State const& state
) :
    SecondaryCmdBuffer{core, queueFamilyIndex} { wrap(state); }

SecondaryCmdBuffer::SecondaryCmdBuffer(
    cvk::Core const& core,
    cvk::queue_family_index_t queueFamilyIndex,
    cvk::create_t
) :
    SecondaryCmdBuffer{core, queueFamilyIndex} { create(); }

void SecondaryCmdBuffer::begin(VkCommandBufferUsageFlags usage, cvk::HeapInheritance const& heaps) {
    reset();
    _cmdBuffer.begin(usage, heaps);
}

void SecondaryCmdBuffer::begin(
    Inheritance const& inheritance,
    VkCommandBufferUsageFlags usage,
    cvk::HeapInheritance const& heaps
) {
    reset();
    _cmdBuffer.begin(inheritance, usage, heaps);
}
}
