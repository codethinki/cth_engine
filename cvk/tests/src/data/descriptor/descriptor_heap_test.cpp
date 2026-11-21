#include "context.hpp"
#include "test.hpp"

#include "cvk/base/physical_device.hpp"
#include "cvk/data/buffer/buffer.hpp"
#include "cvk/data/descriptor/resource_heap.hpp"
#include "cvk/data/descriptor/sampler_heap.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/cmd/cmd_pool.hpp"
#include "cvk/render/pipeline/compute_pipeline.hpp"
#include "cvk/render/pipeline/shader/shader.hpp"
#include "cvk/render/sync/barrier.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>


namespace cvk {
namespace {
    [[nodiscard]] VkPhysicalDeviceDescriptorHeapPropertiesEXT const& heap_properties(
        Core const& core
    ) { return core.physicalDevice().descriptorHeapProperties(); }

    [[nodiscard]] constexpr size_t align(size_t value, size_t alignment) {
        return (value + alignment - 1) / alignment * alignment;
    }

    /**
     * @return stride of the image descriptors of the device
     */
    [[nodiscard]] size_t image_stride(Core const& core) {
        auto const& properties = heap_properties(core);
        return align(properties.imageDescriptorSize, properties.imageDescriptorAlignment);
    }

    /**
     * 1x1 `VK_FORMAT_R8G8B8A8_UNORM` image with the usage
     */
    [[nodiscard]] Image pixel(Core const& core, VkImageUsageFlags usage) {
        return Image{core, {.format = VK_FORMAT_R8G8B8A8_UNORM, .usage = usage}, VkExtent2D{1, 1}};
    }
}

CVK_TEST(DescriptorHeap, descriptor_types) {
    static_assert(descriptor<SampledImageDescriptor>);
    static_assert(descriptor<StorageImageDescriptor>);
    static_assert(descriptor<InputAttachmentDescriptor>);
    static_assert(descriptor<SamplerConfig>);
    static_assert(!descriptor<Image>);

    static_assert(std::is_aggregate_v<SampledImageDescriptor>);
    static_assert(std::is_aggregate_v<ResourceHeap::Config>);
    static_assert(std::is_aggregate_v<SamplerHeap::Config>);

    static_assert(std::same_as<heap_of_t<StorageImageDescriptor>, ResourceHeap>);
    static_assert(std::same_as<heap_of_t<SamplerConfig>, SamplerHeap>);
    static_assert(descriptor_of<InputAttachmentDescriptor, ResourceHeap>);
    static_assert(!descriptor_of<SamplerConfig, ResourceHeap>);
}

namespace {
    template<class Heap>
    concept writes_through_a_span = requires(Heap& heap) { heap.sampledImages(); };

    template<class Heap>
    concept writes_directly = requires(Heap& heap, SampledImageDescriptor const& descriptor) {
        heap.write(0, descriptor);
    };
}

CVK_TEST(DescriptorHeap, a_span_views_the_heap_of_its_type) {
    using sampled_span = DescriptorSpan<SampledImageDescriptor>;
    using sampler_span = DescriptorSpan<SamplerConfig>;

    static_assert(std::is_constructible_v<sampled_span, ResourceHeap&>);
    static_assert(!std::is_constructible_v<sampled_span, SamplerHeap&>);
    static_assert(!std::is_convertible_v<ResourceHeap&, sampled_span>);
    static_assert(std::is_constructible_v<sampler_span, SamplerHeap&>);
    static_assert(!std::is_constructible_v<sampler_span, ResourceHeap&>);

    // a const heap writes nothing
    static_assert(!std::is_constructible_v<sampled_span, ResourceHeap const&>);
    static_assert(!writes_through_a_span<ResourceHeap const>);
    static_assert(writes_through_a_span<ResourceHeap>);
    static_assert(!writes_directly<ResourceHeap const>);
    static_assert(writes_directly<ResourceHeap>);
}

CVK_TEST(DescriptorHeap, references_are_write_only) {
    using reference = DescriptorSpan<SampledImageDescriptor>::reference;

    static_assert(std::is_assignable_v<reference const&, SampledImageDescriptor const&>);
    static_assert(!std::is_copy_assignable_v<reference>);
    static_assert(!std::is_assignable_v<reference const&, StorageImageDescriptor const&>);
}

CVK_TEST(ResourceHeap, base_constructor_does_not_create) {
    test::Context const context{};

    ResourceHeap heap{context.core, {.sampledImages = 1}};

    EXPECT_FALSE(heap.created());
    EXPECT_EQ(&heap.core(), &context.core);
}

CVK_TEST(ResourceHeap, create_lays_out_the_areas_and_the_reserved_range) {
    test::Context const context{};
    auto const& properties = heap_properties(context.core);
    auto const stride = image_stride(context.core);

    ResourceHeap heap{
        context.core,
        {.sampledImages = 3, .storageImages = 2, .inputAttachments = 1},
        create
    };
    ASSERT_TRUE(heap.created());

    EXPECT_EQ(heap.sampledImages().size(), 3);
    EXPECT_EQ(heap.storageImages().size(), 2);
    EXPECT_EQ(heap.inputAttachments().size(), 1);

    auto const info = heap.bindInfo();
    auto const reservedAlignment = std::max(
        properties.bufferDescriptorAlignment,
        properties.imageDescriptorAlignment
    );
    EXPECT_EQ(info.heapRange.address % properties.resourceHeapAlignment, 0);
    EXPECT_EQ(info.reservedRangeOffset, align(6 * stride, reservedAlignment));
    EXPECT_EQ(info.reservedRangeSize, properties.minResourceHeapReservedRange);
    // the reserved range lies at the end
    EXPECT_EQ(info.reservedRangeOffset + info.reservedRangeSize, info.heapRange.size);
}

CVK_TEST(ResourceHeap, a_heap_above_the_device_limit_throws) {
    test::Context const context{};
    ResourceHeap heap{context.core, {.sampledImages = std::numeric_limits<uint32_t>::max()}};

    EXPECT_THROW(heap.create(), cvk_exception);
    EXPECT_FALSE(heap.created());
}

CVK_TEST(ResourceHeap, destroy_and_create_again) {
    test::Context const context{};
    ResourceHeap heap{context.core, {.storageImages = 1}, create};

    heap.destroy();
    EXPECT_FALSE(heap.created());

    heap.create();
    EXPECT_TRUE(heap.created());
}

CVK_TEST(ResourceHeap, move_assignment_destroys_the_old_heap) {
    test::Context const context{};
    ResourceHeap heap{context.core, {.sampledImages = 1}, create};
    ResourceHeap other{context.core, {.sampledImages = 2}, create};
    auto const address = other.bindInfo().heapRange.address;

    heap = std::move(other);

    EXPECT_EQ(heap.bindInfo().heapRange.address, address);
    EXPECT_EQ(heap.sampledImages().size(), 2);
    EXPECT_FALSE(other.created());
}

CVK_TEST(SamplerHeap, create_lays_out_the_samplers_and_the_reserved_range) {
    test::Context const context{};
    auto const& properties = heap_properties(context.core);
    auto const stride = align(
        properties.samplerDescriptorSize,
        properties.samplerDescriptorAlignment
    );

    SamplerHeap heap{context.core, {.samplers = 5}, create};
    ASSERT_TRUE(heap.created());
    EXPECT_EQ(heap.samplers().size(), 5);

    auto const info = heap.bindInfo();
    EXPECT_EQ(info.heapRange.address % properties.samplerHeapAlignment, 0);
    EXPECT_EQ(info.reservedRangeOffset, align(5 * stride, properties.samplerDescriptorAlignment));
    EXPECT_EQ(info.reservedRangeSize, properties.minSamplerHeapReservedRange);
    EXPECT_EQ(info.reservedRangeOffset + info.reservedRangeSize, info.heapRange.size);
}

CVK_TEST(SamplerHeap, a_heap_above_the_device_limit_throws) {
    test::Context const context{};
    SamplerHeap heap{context.core, {.samplers = std::numeric_limits<uint32_t>::max()}};

    EXPECT_THROW(heap.create(), cvk_exception);
    EXPECT_FALSE(heap.created());
}

CVK_TEST(DescriptorSpan, mapping_points_at_the_area_of_the_span) {
    test::Context const context{};
    auto const stride = image_stride(context.core);
    ResourceHeap heap{
        context.core,
        {.sampledImages = 3, .storageImages = 2, .inputAttachments = 1},
        create
    };

    auto const sampled = heap.sampledImages().mapping(1, 4);
    EXPECT_EQ(sampled.sType, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT);
    EXPECT_EQ(sampled.descriptorSet, 1);
    EXPECT_EQ(sampled.firstBinding, 4);
    EXPECT_EQ(sampled.bindingCount, 1);
    EXPECT_EQ(sampled.resourceMask, VK_SPIRV_RESOURCE_TYPE_SAMPLED_IMAGE_BIT_EXT);
    EXPECT_EQ(sampled.source, VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT);
    EXPECT_EQ(sampled.sourceData.constantOffset.heapOffset, 0);
    EXPECT_EQ(sampled.sourceData.constantOffset.heapArrayStride, stride);
    EXPECT_EQ(sampled.sourceData.constantOffset.pEmbeddedSampler, nullptr);

    constexpr VkSpirvResourceTypeFlagsEXT IMAGES = VK_SPIRV_RESOURCE_TYPE_READ_ONLY_IMAGE_BIT_EXT
        | VK_SPIRV_RESOURCE_TYPE_READ_WRITE_IMAGE_BIT_EXT;

    auto const storage = heap.storageImages().mapping(0, 0);
    EXPECT_EQ(storage.resourceMask, IMAGES);
    EXPECT_EQ(storage.sourceData.constantOffset.heapOffset, 3 * stride);
    EXPECT_EQ(storage.sourceData.constantOffset.heapArrayStride, stride);

    auto const input = heap.inputAttachments().mapping(0, 0);
    EXPECT_EQ(input.resourceMask, IMAGES);
    EXPECT_EQ(input.sourceData.constantOffset.heapOffset, 5 * stride);
}

CVK_TEST(DescriptorSpan, sampler_mapping) {
    test::Context const context{};
    auto const& properties = heap_properties(context.core);
    SamplerHeap heap{context.core, {.samplers = 2}, create};

    auto const mapping = heap.samplers().mapping(0, 1);

    EXPECT_EQ(mapping.resourceMask, VK_SPIRV_RESOURCE_TYPE_SAMPLER_BIT_EXT);
    EXPECT_EQ(mapping.sourceData.constantOffset.heapOffset, 0);
    EXPECT_EQ(
        mapping.sourceData.constantOffset.heapArrayStride,
        align(properties.samplerDescriptorSize, properties.samplerDescriptorAlignment)
    );
}

CVK_TEST(DescriptorSpan, writes_every_descriptor_type) {
    test::Context const context{};
    ResourceHeap resources{
        context.core,
        {.sampledImages = 2, .storageImages = 2, .inputAttachments = 2},
        create
    };
    SamplerHeap samplers{context.core, {.samplers = 2}, create};
    auto const sampled = pixel(context.core, VK_IMAGE_USAGE_SAMPLED_BIT);
    auto const storage = pixel(context.core, VK_IMAGE_USAGE_STORAGE_BIT);
    auto const input = pixel(
        context.core,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
    );

    // the last cell of every span
    resources.sampledImages()[1] = SampledImageDescriptor{sampled};
    resources.storageImages()[1] = StorageImageDescriptor{storage};
    resources.inputAttachments()[1] = InputAttachmentDescriptor{input};
    samplers.samplers()[1] = SamplerConfig{.magFilter = VK_FILTER_NEAREST, .maxAnisotropy = 0.f};

    // a cell takes a new descriptor
    resources.sampledImages()[1] = SampledImageDescriptor{
        .image = sampled,
        .layout = VK_IMAGE_LAYOUT_GENERAL
    };

    // the heap writes without a span
    resources.write(0, SampledImageDescriptor{sampled});
    resources.write(0, StorageImageDescriptor{storage});
    resources.write(0, InputAttachmentDescriptor{input});
    samplers.write(0, SamplerConfig{});
}

CVK_TEST(DescriptorSpan, mapping_is_the_mapping_of_the_heap) {
    test::Context const context{};
    ResourceHeap resources{context.core, {.sampledImages = 2, .storageImages = 1}, create};
    SamplerHeap samplers{context.core, {.samplers = 1}, create};

    auto const span = resources.storageImages().mapping(0, 3);
    auto const heap = resources.mapping<StorageImageDescriptor>(0, 3);
    EXPECT_EQ(span.firstBinding, heap.firstBinding);
    EXPECT_EQ(span.resourceMask, heap.resourceMask);
    EXPECT_EQ(span.sourceData.constantOffset.heapOffset, heap.sourceData.constantOffset.heapOffset);
    EXPECT_EQ(
        span.sourceData.constantOffset.heapArrayStride,
        heap.sourceData.constantOffset.heapArrayStride
    );

    EXPECT_EQ(
        samplers.samplers().mapping(0, 1).sourceData.constantOffset.heapArrayStride,
        samplers.mapping<SamplerConfig>(0, 1).sourceData.constantOffset.heapArrayStride
    );
}

#ifdef CTH_DEBUG_MODE
CVK_TEST(DescriptorSpan, index_out_of_the_span_crashes) {
    // the child runs the test again: the parent has device threads
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    test::Context const context{};
    SamplerHeap heap{context.core, {.samplers = 2}, create};

    ASSERT_DEATH(
        [[maybe_unused]]
        auto const cell = heap.samplers()[2],
        ".*"
    );
}
#endif

namespace {
    /**
     * heaps, texture and storage image of the heap compute shader
     * @details texture: green at sampled image 3, sampler 2, storage image 1
     */
    struct HeapCompute {
        static constexpr uint32_t TEXTURE_IDX = 3;
        static constexpr uint32_t SAMPLER_IDX = 2;
        static constexpr uint32_t IMAGE_IDX = 1;

        explicit HeapCompute(Core const& core) : resources{
                core,
                {.sampledImages = 4, .storageImages = 2},
                create
            },
            samplers{core, {.samplers = 3}, create},
            texture{pixel(core, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)},
            image{pixel(core, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)},
            shader{core, VK_SHADER_STAGE_COMPUTE_BIT, test::SHADER_DIR / "heap.comp.spv"},
            pipeline{
                core,
                {
                    .stage = shader.stageInfo(),
                    .descriptorMappings = {
                        resources.sampledImages().mapping(0, 0),
                        samplers.samplers().mapping(0, 1),
                        resources.storageImages().mapping(0, 2),
                    },
                },
                create
            },
            staging{core, test::STAGING_CONFIG, 4},
            readback{core, test::READBACK_CONFIG, 4} {
            staging.write(test::GREEN_PIXEL);
            resources.sampledImages()[TEXTURE_IDX] = SampledImageDescriptor{texture};
            samplers.samplers()[SAMPLER_IDX] = SamplerConfig{};
            resources.storageImages()[IMAGE_IDX] = StorageImageDescriptor{image};
        }

        /**
         * records the upload of the texture and the transitions for the dispatch
         */
        void prepare(BaseCmdBuffer const& cmd) const {
            Barrier toTransfer{};
            toTransfer.addTransition(
                texture,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_UNDEFINED
            );
            toTransfer.execute(cmd);
            texture.copy(cmd, staging);

            Barrier toShader{};
            toShader.add(
                texture,
                ImageBarrierInfo::LayoutTransition(
                    VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT},
                    {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT}
                )
            );
            toShader.add(
                image,
                ImageBarrierInfo::LayoutTransition(
                    VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT},
                    {VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE}
                )
            );
            toShader.execute(cmd);
        }

        /**
         * records the pipeline, the indices and the dispatch
         * @pre the heaps are bound or inherited
         */
        void dispatch(BaseCmdBuffer const& cmd) const {
            cmd.bind(pipeline);
            cmd.pushData(std::array{TEXTURE_IDX, SAMPLER_IDX, IMAGE_IDX});
            cmd.dispatch(1);
        }

        /**
         * records the copy of the storage image into the readback buffer
         */
        void read(test::Context const& context, BaseCmdBuffer const& cmd) const {
            Barrier toTransfer{};
            toTransfer.add(
                image,
                ImageBarrierInfo::LayoutTransition(
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_IMAGE_LAYOUT_GENERAL,
                    {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT},
                    {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT}
                )
            );
            toTransfer.execute(cmd);

            test::copy_to_buffer(context, cmd, image, readback, 0, 0);
            test::host_read_barrier(context, cmd);
        }

        ResourceHeap resources;
        SamplerHeap samplers;
        Image texture;
        Image image;
        Shader shader;
        ComputePipeline pipeline;
        Buffer<unsigned char> staging;
        BaseBuffer readback;
    };
}

CVK_TEST(DescriptorHeap, compute_samples_a_texture_and_stores_into_an_image_from_the_heaps) {
    test::Context const context{};
    HeapCompute compute{context.core};

    test::record_and_submit(
        context,
        [&](PrimaryCmdBuffer const& cmd) {
            compute.prepare(cmd);
            cmd.bind(compute.resources);
            cmd.bind(compute.samplers);
            compute.dispatch(cmd);
            compute.read(context, cmd);
        }
    );

    EXPECT_TRUE(test::every_pixel_is(compute.readback, test::GREEN_PIXEL));
}

CVK_TEST(DescriptorHeap, secondary_inherits_the_heaps_of_the_primary) {
    test::Context const context{};
    HeapCompute compute{context.core};
    CmdPool const pool{context.core, test::pool_config(context.core), create};
    SecondaryCmdBuffer secondary{pool, create};

    secondary.begin(
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        {.resourceHeapPtr = &compute.resources, .samplerHeapPtr = &compute.samplers}
    );
    compute.dispatch(secondary);
    secondary.end();

    test::record_and_submit(
        context,
        [&](PrimaryCmdBuffer const& cmd) {
            compute.prepare(cmd);
            cmd.bind(compute.resources);
            cmd.bind(compute.samplers);
            cmd.execute(secondary);
            compute.read(context, cmd);
        }
    );

    EXPECT_TRUE(test::every_pixel_is(compute.readback, test::GREEN_PIXEL));
}
}
