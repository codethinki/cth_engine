# Test needs

N no Vulkan, F fake function table, S one shared real device per process, O own real instance or device

Totals: N 67, F 189, S 46, O 5, all 307

## base/allocator_test.cpp

|   | test                                       | reason                                   |
|---|--------------------------------------------|------------------------------------------|
| F | core_creates_the_allocator                 | cvk logic: lifecycle, state or arguments |
| F | base_constructor_does_not_create           | cvk logic: lifecycle, state or arguments |
| F | create_and_destroy                         | cvk logic: lifecycle, state or arguments |
| F | release_and_wrap                           | cvk logic: lifecycle, state or arguments |
| F | move_assignment_destroys_the_old_allocator | cvk logic: lifecycle, state or arguments |
| F | flags_follow_the_enabled_features          | cvk logic: lifecycle, state or arguments |
| F | no_flags_without_the_features              | cvk logic: lifecycle, state or arguments |

## base/core_test.cpp

|   | test                                               | reason                                     |
|---|----------------------------------------------------|--------------------------------------------|
| F | base_constructor_does_not_create                   | cvk logic: lifecycle, state or arguments   |
| O | create_without_queue_sets                          | one real core: pick, device, VMA           |
| F | create_one_queue_per_entry                         | skips today, fake families run everywhere  |
| F | queue_set_group_shares_a_queue                     | cvk logic: lifecycle, state or arguments   |
| F | first_possible_queue_set_wins                      | cvk logic: lifecycle, state or arguments   |
| F | no_suitable_device_throws                          | cvk logic: lifecycle, state or arguments   |
| F | release_and_wrap                                   | cvk logic: lifecycle, state or arguments   |
| F | wrap_without_queue_indices_takes_the_device_queues | cvk logic: lifecycle, state or arguments   |
| F | wrap_without_allocator_creates_one                 | cvk logic: lifecycle, state or arguments   |
| F | queues_work_after_move                             | cvk logic: lifecycle, state or arguments   |
| O | present_queue_with_headless_surface                | real present support of a headless surface |
| F | move_assignment_destroys_the_old_core              | records the destroy calls                  |

## base/device_test.cpp

|   | test                                        | reason                                                       |
|---|---------------------------------------------|--------------------------------------------------------------|
| F | base_constructor_does_not_create            | cvk logic: lifecycle, state or arguments                     |
| O | create_retrieves_the_queue                  | one real vkCreateDevice                                      |
| F | create_retrieves_the_queues                 | skips today on a one queue GPU, fake families run everywhere |
| F | queues_of_one_family_get_increasing_indices | skips today, fake families run everywhere                    |
| F | queues_work_after_move                      | cvk logic: lifecycle, state or arguments                     |
| F | release_and_wrap                            | cvk logic: lifecycle, state or arguments                     |
| F | wrap_without_table_loads_it                 | cvk logic: lifecycle, state or arguments                     |
| F | release_twice_is_safe                       | cvk logic: lifecycle, state or arguments                     |
| F | present_enables_swapchain_maintenance1      | cvk logic: lifecycle, state or arguments                     |
| F | move_assignment_destroys_the_old_device     | records the destroy calls                                    |

## base/instance_test.cpp

|   | test                                               | reason                                               |
|---|----------------------------------------------------|------------------------------------------------------|
| N | base_constructor_does_not_create                   | no Vulkan                                            |
| O | create_and_destroy                                 | one real instance with layers and volk               |
| F | constructor_creates                                | cvk logic: lifecycle, state or arguments             |
| F | create_twice_loads_the_new_instance                | cvk logic: lifecycle, state or arguments             |
| F | create_enables_the_extensions_of_the_config        | cvk logic: lifecycle, state or arguments             |
| F | create_with_a_missing_extension_throws_and_forgets | cvk logic: lifecycle, state or arguments             |
| O | available_extensions_contain_the_surface           | real loader query, no instance, cheap                |
| F | wrap_takes_ownership                               | cvk logic: lifecycle, state or arguments             |
| F | move_assignment_takes_the_other_instance           | records the destroy order: messenger before instance |
| F | release_and_wrap                                   | cvk logic: lifecycle, state or arguments             |

## base/physical_device_test.cpp

|   | test                                        | reason                                   |
|---|---------------------------------------------|------------------------------------------|
| F | pick_finds_the_queues                       | cvk logic: lifecycle, state or arguments |
| S | picked_device_has_the_features              | the real driver has the base features    |
| F | missing_names_the_extensions                | cvk logic: lifecycle, state or arguments |
| F | pick_with_a_missing_extension_finds_nothing | cvk logic: lifecycle, state or arguments |
| F | pick_present_without_surfaces_finds_nothing | cvk logic: lifecycle, state or arguments |
| F | pick_present_finds_a_family_of_the_surface  | cvk logic: lifecycle, state or arguments |
| F | enumerate_without_surfaces_has_no_present   | cvk logic: lifecycle, state or arguments |
| F | copy_keeps_the_queried_members              | cvk logic: lifecycle, state or arguments |
| F | find_supported_format                       | cvk logic: lifecycle, state or arguments |

## base/queue/queue_test.cpp

|   | test                                                    | reason                                   |
|---|---------------------------------------------------------|------------------------------------------|
| F | copies_share_the_queue                                  | cvk logic: lifecycle, state or arguments |
| S | raw_submit_signals_the_fence                            | the gpu signals the fence                |
| F | raw_submit_without_fence                                | cvk logic: lifecycle, state or arguments |
| F | copies_keep_the_queue_through_a_device_release_and_wrap | cvk logic: lifecycle, state or arguments |
| F | present_family_can_present                              | cvk logic: lifecycle, state or arguments |

## base/queue/submit_test.cpp

|   | test                                             | reason                                   |
|---|--------------------------------------------------|------------------------------------------|
| F | semaphore_info_takes_the_kind_from_the_semaphore | cvk logic: lifecycle, state or arguments |
| F | command_buffer_without_fence                     | cvk logic: lifecycle, state or arguments |
| S | binary_semaphore_orders_two_infos                | the layers check the semaphore order     |
| S | timeline_signal_takes_the_next_value             | the gpu signals the value                |
| F | timeline_values_resolve_in_order                 | records the values of the submit         |
| F | explicit_timeline_value_keeps_the_counter        | records the values of the submit         |

## data/buffer/base_buffer_test.cpp

|   | test                                                 | reason                                    |
|---|------------------------------------------------------|-------------------------------------------|
| N | config_create_info                                   | no Vulkan                                 |
| N | config_one_queue_family_is_exclusive                 | no Vulkan                                 |
| N | config_queue_families_are_concurrent                 | no Vulkan                                 |
| N | config_debug_check_is_constexpr                      | no Vulkan                                 |
| F | base_constructor_does_not_create                     | cvk logic: lifecycle, state or arguments  |
| F | create_and_destroy                                   | cvk logic: lifecycle, state or arguments  |
| F | create_concurrent                                    | skips today, fake families run everywhere |
| S | auto_usage_picks_host_visible_memory_for_host_access | real VMA memory choice                    |
| F | write_and_read                                       | cvk logic: lifecycle, state or arguments  |
| S | copy_a_range                                         | gpu result                                |
| F | copy_whole_size_fits_both_buffers                    | records the copy size                     |
| F | copy_the_whole_source_to_an_offset                   | records the copy region                   |
| F | release_and_wrap                                     | cvk logic: lifecycle, state or arguments  |
| F | wrap_without_memory_destroys_only_the_buffer         | cvk logic: lifecycle, state or arguments  |
| F | move_assignment_destroys_the_old_buffer              | cvk logic: lifecycle, state or arguments  |

## data/buffer/buffer_test.cpp

|   | test                                    | reason                                   |
|---|-----------------------------------------|------------------------------------------|
| N | elements_must_be_trivially_copyable     | no Vulkan                                |
| N | adds_no_members                         | no Vulkan                                |
| F | create_counts_elements                  | cvk logic: lifecycle, state or arguments |
| F | write_and_map                           | cvk logic: lifecycle, state or arguments |
| F | map_whole_buffer                        | cvk logic: lifecycle, state or arguments |
| S | copy_elements                           | gpu result                               |
| F | copy_the_whole_source_to_an_element     | records the copy region in bytes         |
| F | release_and_wrap_as_bytes               | cvk logic: lifecycle, state or arguments |
| F | move_assignment_destroys_the_old_buffer | cvk logic: lifecycle, state or arguments |

## data/descriptor/descriptor_draw_test.cpp

|   | test                                | reason                                         |
|---|-------------------------------------|------------------------------------------------|
| S | uniform_buffer_color                | gpu result                                     |
| S | bindless_texture_written_after_bind | gpu result                                     |
| S | texture_in_the_layout_of_the_write  | gpu result, layers check the descriptor layout |

## data/descriptor/descriptor_pool_test.cpp

|   | test                                                      | reason                                   |
|---|-----------------------------------------------------------|------------------------------------------|
| F | config_add_counts_the_descriptors_of_the_layout           | cvk logic: lifecycle, state or arguments |
| F | config_add_takes_the_variable_count_and_update_after_bind | cvk logic: lifecycle, state or arguments |
| F | constructor_counts_every_layout                           | cvk logic: lifecycle, state or arguments |
| F | create_and_destroy                                        | cvk logic: lifecycle, state or arguments |
| F | reset_counts                                              | cvk logic: lifecycle, state or arguments |
| F | release_and_wrap                                          | cvk logic: lifecycle, state or arguments |

## data/descriptor/descriptor_set_test.cpp

|   | test                                      | reason                                                        |
|---|-------------------------------------------|---------------------------------------------------------------|
| F | base_constructor_does_not_create          | cvk logic: lifecycle, state or arguments                      |
| F | destroy_frees_with_a_freeing_pool         | cvk logic: lifecycle, state or arguments                      |
| F | destroy_without_free_only_forgets         | cvk logic: lifecycle, state or arguments                      |
| F | pool_reset_frees_the_set                  | cvk logic: lifecycle, state or arguments                      |
| F | write_buffer_and_image                    | records the writes, the draw tests check them with the layers |
| F | write_one_descriptor_and_the_whole_buffer | records the writes                                            |
| F | variable_count                            | cvk logic: lifecycle, state or arguments                      |
| F | release_and_wrap                          | cvk logic: lifecycle, state or arguments                      |

## data/img/image_test.cpp

|   | test                                        | reason                                    |
|---|---------------------------------------------|-------------------------------------------|
| N | eval_mip_level_count                        | no Vulkan                                 |
| N | format_aspects                              | no Vulkan                                 |
| N | config_aspect_mask_fits_the_format          | no Vulkan                                 |
| F | aspect_mask_comes_from_the_format           | cvk logic: lifecycle, state or arguments  |
| N | config_create_info                          | no Vulkan                                 |
| N | config_queue_families_are_concurrent        | no Vulkan                                 |
| N | config_depth_buffer                         | no Vulkan                                 |
| F | base_constructor_does_not_create            | cvk logic: lifecycle, state or arguments  |
| F | create_and_destroy                          | cvk logic: lifecycle, state or arguments  |
| F | create_depth_buffer                         | cvk logic: lifecycle, state or arguments  |
| S | copy_from_a_buffer_into_two_mip_levels      | gpu result, layers check the layouts      |
| S | copy_two_mip_levels_to_a_buffer             | gpu result, layers check the layouts      |
| S | typed_buffer_copies_a_mip_level_in_elements | gpu result, layers check the layouts      |
| S | blit_mip_levels_fills_the_mip_chain         | gpu result, layers check layouts and sync |
| F | release_and_wrap                            | cvk logic: lifecycle, state or arguments  |
| F | wrap_without_memory_destroys_only_the_image | cvk logic: lifecycle, state or arguments  |
| F | move_assignment_destroys_the_old_image      | cvk logic: lifecycle, state or arguments  |

## data/img/image_view_test.cpp

|   | test                                     | reason                                   |
|---|------------------------------------------|------------------------------------------|
| N | config_range                             | no Vulkan                                |
| F | base_constructor_does_not_create         | cvk logic: lifecycle, state or arguments |
| F | create_and_destroy                       | cvk logic: lifecycle, state or arguments |
| F | create_on_a_mip_range                    | cvk logic: lifecycle, state or arguments |
| F | depth_view_takes_the_aspect_of_the_image | cvk logic: lifecycle, state or arguments |
| F | release_and_wrap                         | cvk logic: lifecycle, state or arguments |
| F | move_assignment_destroys_the_old_view    | cvk logic: lifecycle, state or arguments |

## data/img/sampler_test.cpp

|   | test                                     | reason                                   |
|---|------------------------------------------|------------------------------------------|
| N | config_create_info                       | no Vulkan                                |
| N | config_create_info_is_constexpr          | no Vulkan                                |
| F | base_constructor_clamps_anisotropy       | fake limit                               |
| F | create_default                           | cvk logic: lifecycle, state or arguments |
| F | create_compare_without_anisotropy        | cvk logic: lifecycle, state or arguments |
| F | release_and_wrap                         | cvk logic: lifecycle, state or arguments |
| F | move_assignment_destroys_the_old_sampler | cvk logic: lifecycle, state or arguments |

## data/memory/memory_test.cpp

|   | test                                    | reason                                   |
|---|-----------------------------------------|------------------------------------------|
| N | config_create_info                      | no Vulkan                                |
| N | config_auto_usage                       | no Vulkan                                |
| F | base_constructor_does_not_create        | cvk logic: lifecycle, state or arguments |
| F | create_and_destroy                      | cvk logic: lifecycle, state or arguments |
| S | properties_have_the_required_flags      | real memory types                        |
| F | map_write_and_read                      | cvk logic: lifecycle, state or arguments |
| F | map_a_range                             | cvk logic: lifecycle, state or arguments |
| F | write_copies_the_data                   | cvk logic: lifecycle, state or arguments |
| F | the_mapping_unmaps                      | cvk logic: lifecycle, state or arguments |
| F | a_moved_mapping_unmaps_once             | cvk logic: lifecycle, state or arguments |
| F | mapping_as_elements                     | cvk logic: lifecycle, state or arguments |
| F | mapped_bit_maps_persistently            | cvk logic: lifecycle, state or arguments |
| F | release_and_wrap                        | cvk logic: lifecycle, state or arguments |
| F | move_assignment_destroys_the_old_memory | cvk logic: lifecycle, state or arguments |

## debug/debug_messenger_test.cpp

|   | test                                       | reason                                   |
|---|--------------------------------------------|------------------------------------------|
| S | forwards_messages_to_the_callback          | a real message of the layers             |
| F | accepts_a_lambda                           | cvk logic: lifecycle, state or arguments |
| F | destroy_resets                             | cvk logic: lifecycle, state or arguments |
| F | release_keeps_the_messenger                | cvk logic: lifecycle, state or arguments |
| F | release_and_wrap                           | cvk logic: lifecycle, state or arguments |
| F | move_assignment_destroys_the_old_messenger | cvk logic: lifecycle, state or arguments |

## render/cmd/cmd_buffer_test.cpp

|   | test                                       | reason                                   |
|---|--------------------------------------------|------------------------------------------|
| N | inheritance_rendering_info                 | no Vulkan                                |
| N | push_constant_data                         | no Vulkan                                |
| N | delete_only_through_the_derived_class      | no Vulkan                                |
| F | base_constructor_does_not_create           | cvk logic: lifecycle, state or arguments |
| F | create_and_destroy                         | cvk logic: lifecycle, state or arguments |
| F | begin_and_end                              | cvk logic: lifecycle, state or arguments |
| S | submit                                     | the gpu runs the buffer                  |
| F | reset_and_record_again                     | cvk logic: lifecycle, state or arguments |
| S | execute_secondaries                        | layers check the inheritance             |
| F | secondary_begin_with_rendering_inheritance | cvk logic: lifecycle, state or arguments |
| F | release_and_wrap                           | cvk logic: lifecycle, state or arguments |
| F | moved_from_buffer_does_not_record          | cvk logic: lifecycle, state or arguments |
| F | move_assignment_frees_the_old_buffer       | cvk logic: lifecycle, state or arguments |

## render/cmd/cmd_pool_test.cpp

|   | test                                           | reason                                   |
|---|------------------------------------------------|------------------------------------------|
| N | config_create_info                             | no Vulkan                                |
| F | config_from_the_queue_uses_its_family          | cvk logic: lifecycle, state or arguments |
| F | base_constructor_does_not_create               | cvk logic: lifecycle, state or arguments |
| F | create_and_destroy                             | cvk logic: lifecycle, state or arguments |
| F | counts_its_command_buffers                     | cvk logic: lifecycle, state or arguments |
| F | release_of_a_buffer_stops_the_count            | cvk logic: lifecycle, state or arguments |
| F | owner_of_a_released_buffer_counts_it           | cvk logic: lifecycle, state or arguments |
| F | reset_returns_the_buffers_to_the_initial_state | cvk logic: lifecycle, state or arguments |
| F | reset_ends_the_recording                       | cvk logic: lifecycle, state or arguments |
| F | release_and_wrap                               | cvk logic: lifecycle, state or arguments |
| F | move_assignment_destroys_the_old_pool          | cvk logic: lifecycle, state or arguments |

## render/cmd/draw_test.cpp

|   | test                           | reason                      |
|---|--------------------------------|-----------------------------|
| N | index_type_follows_the_element | no Vulkan                   |
| S | draw                           | gpu result                  |
| S | draw_indexed                   | layers check the draw state |
| S | draw_indirect                  | layers check the draw state |
| S | draw_indexed_indirect          | layers check the draw state |

## render/cmd/local_read_test.cpp

|   | test                                           | reason     |
|---|------------------------------------------------|------------|
| N | empty_maps_to_the_default                      | no Vulkan  |
| N | infos_point_into_the_mapping                   | no Vulkan  |
| N | empty_equals_the_default                       | no Vulkan  |
| S | draw_reads_an_attachment_of_the_same_rendering | gpu result |
| S | secondary_inherits_the_mapping                 | gpu result |

## render/cmd/rendering_test.cpp

|   | test                                   | reason                                   |
|---|----------------------------------------|------------------------------------------|
| S | clear_a_color_attachment               | gpu result                               |
| F | tracks_the_current_rendering           | cvk logic: lifecycle, state or arguments |
| S | clear_a_depth_attachment               | layers check the rendering               |
| S | depth_attachment_in_the_default_layout | layers check the layout                  |
| S | secondary_inherits_the_rendering       | layers check the inheritance             |
| S | local_read_inside_the_rendering        | layers check the local read barrier      |

## render/pipeline/layout/descriptor_set_layout_test.cpp

|   | test                                        | reason                                   |
|---|---------------------------------------------|------------------------------------------|
| N | config_sorts_bindings                       | no Vulkan                                |
| N | config_remove                               | no Vulkan                                |
| N | config_is_constexpr                         | no Vulkan                                |
| F | create_with_gaps                            | cvk logic: lifecycle, state or arguments |
| F | create_empty                                | cvk logic: lifecycle, state or arguments |
| F | release_and_wrap                            | cvk logic: lifecycle, state or arguments |
| F | move_assignment_destroys_the_old_layout     | cvk logic: lifecycle, state or arguments |
| N | config_keeps_the_flags_in_binding_order     | no Vulkan                                |
| N | config_keeps_the_flags_of_the_first_binding | no Vulkan                                |
| N | config_without_flags_keeps_no_flags         | no Vulkan                                |
| F | create_bindless                             | cvk logic: lifecycle, state or arguments |

## render/pipeline/layout/pipeline_layout_test.cpp

|   | test                                      | reason                                   |
|---|-------------------------------------------|------------------------------------------|
| F | create_empty                              | cvk logic: lifecycle, state or arguments |
| F | create_with_sets_and_push_constants       | cvk logic: lifecycle, state or arguments |
| F | too_many_sets_throws                      | fake limit                               |
| F | set_layouts_can_be_destroyed_after_create | cvk logic: lifecycle, state or arguments |
| F | release_and_wrap                          | cvk logic: lifecycle, state or arguments |
| F | move_assignment_destroys_the_old_layout   | cvk logic: lifecycle, state or arguments |

## render/pipeline/pipeline_test.cpp

|   | test                                      | reason                                                   |
|---|-------------------------------------------|----------------------------------------------------------|
| N | config_from_the_formats                   | no Vulkan                                                |
| N | config_is_constexpr                       | no Vulkan                                                |
| F | config_stages                             | cvk logic: lifecycle, state or arguments                 |
| S | bind                                      | layers check the bind                                    |
| F | base_constructor_does_not_create          | cvk logic: lifecycle, state or arguments                 |
| S | create                                    | the driver compiles the pipeline, layers check the state |
| S | create_without_depth                      | the driver compiles the pipeline, layers check the state |
| F | shaders_can_be_destroyed_after_create     | cvk logic: lifecycle, state or arguments                 |
| F | release_and_wrap                          | cvk logic: lifecycle, state or arguments                 |
| F | move_assignment_destroys_the_old_pipeline | cvk logic: lifecycle, state or arguments                 |
| F | derivative                                | cvk logic: lifecycle, state or arguments                 |

## render/pipeline/shader/shader_test.cpp

|   | test                                    | reason                                   |
|---|-----------------------------------------|------------------------------------------|
| F | base_constructor_does_not_create        | cvk logic: lifecycle, state or arguments |
| F | create_from_spv                         | cvk logic: lifecycle, state or arguments |
| F | create_from_file                        | cvk logic: lifecycle, state or arguments |
| N | load_rejects_bad_files                  | no Vulkan                                |
| F | release_and_wrap                        | cvk logic: lifecycle, state or arguments |
| F | move_assignment_destroys_the_old_shader | cvk logic: lifecycle, state or arguments |
| F | compile_and_create                      | cvk logic: lifecycle, state or arguments |
| N | compile_error_throws                    | no Vulkan                                |
| N | compile_missing_glsl_throws             | no Vulkan                                |

## render/sync/barrier_test.cpp

|   | test                                                        | reason                                   |
|---|-------------------------------------------------------------|------------------------------------------|
| N | layout_transition_from_undefined_waits_for_the_dst_stages   | no Vulkan                                |
| N | layout_transition_src_from_the_old_dst_from_the_new_layout  | no Vulkan                                |
| N | layout_transition_src_keeps_only_the_writes                 | no Vulkan                                |
| N | layout_transition_aspects_pick_color_or_depth               | no Vulkan                                |
| N | layout_transition_shader_stages_replace_the_fragment_shader | no Vulkan                                |
| N | every_pair_of_known_layouts_has_a_transition                | no Vulkan                                |
| N | layout_transition_info_covers_the_remaining_mip_levels      | no Vulkan                                |
| N | queue_transition_info_keeps_the_layout                      | no Vulkan                                |
| N | memory_barriers_are_constexpr                               | no Vulkan                                |
| N | aggregate_is_constexpr                                      | no Vulkan                                |
| F | add_and_clear                                               | cvk logic: lifecycle, state or arguments |
| S | execute_memory_and_buffer_barriers                          | layers check the barriers                |
| S | image_layout_transition                                     | layers check the barriers                |
| S | add_transition_on_a_mip_range                               | layers check the layouts per mip level   |
| S | memory_only_image_barrier                                   | layers check the barriers                |

## render/sync/fence_test.cpp

|   | test                                   | reason                                   |
|---|----------------------------------------|------------------------------------------|
| F | base_constructor_does_not_create       | cvk logic: lifecycle, state or arguments |
| F | create_not_signaled                    | cvk logic: lifecycle, state or arguments |
| F | create_signaled_and_reset              | cvk logic: lifecycle, state or arguments |
| S | submit_signals                         | the gpu signals the fence                |
| F | wait_reset_timeout_keeps_the_state     | cvk logic: lifecycle, state or arguments |
| F | release_and_wrap                       | cvk logic: lifecycle, state or arguments |
| F | move_assignment_destroys_the_old_fence | cvk logic: lifecycle, state or arguments |
| S | extract_os_handle_waits_for_the_submit | real OS handle                           |
| S | extract_os_handle_of_a_signaled_fence  | real OS handle                           |
| F | release_and_wrap_keep_the_export_types | cvk logic: lifecycle, state or arguments |

## render/sync/semaphore_test.cpp

|   | test                                       | reason                                   |
|---|--------------------------------------------|------------------------------------------|
| F | base_constructor_does_not_create           | cvk logic: lifecycle, state or arguments |
| F | create_and_destroy                         | cvk logic: lifecycle, state or arguments |
| S | submit_signals_and_waits                   | gpu, layers check the order              |
| F | release_and_wrap                           | cvk logic: lifecycle, state or arguments |
| F | move_assignment_destroys_the_old_semaphore | cvk logic: lifecycle, state or arguments |

## render/sync/timeline_semaphore_test.cpp

|   | test                                       | reason                                   |
|---|--------------------------------------------|------------------------------------------|
| F | base_constructor_does_not_create           | cvk logic: lifecycle, state or arguments |
| F | create_sets_the_initial_value              | cvk logic: lifecycle, state or arguments |
| F | signal_advances_the_counter                | cvk logic: lifecycle, state or arguments |
| F | wait_for_a_value_not_signaled_times_out    | cvk logic: lifecycle, state or arguments |
| F | wait_for_an_older_value                    | cvk logic: lifecycle, state or arguments |
| S | submit_signals_the_next_value              | the gpu signals the value                |
| F | release_and_wrap_keep_the_counter          | cvk logic: lifecycle, state or arguments |
| F | move_keeps_the_counter                     | cvk logic: lifecycle, state or arguments |
| F | move_assignment_destroys_the_old_semaphore | cvk logic: lifecycle, state or arguments |
| N | is_not_a_binary_semaphore                  | no Vulkan                                |

## surface/surface_test.cpp

|   | test                                     | reason                                      |
|---|------------------------------------------|---------------------------------------------|
| F | wrap_takes_ownership                     | cvk logic: lifecycle, state or arguments    |
| F | release_keeps_the_surface                | cvk logic: lifecycle, state or arguments    |
| F | empty_config_allows_everything           | cvk logic: lifecycle, state or arguments    |
| F | formats_keep_the_allowed_order           | skips today, fake formats run everywhere    |
| F | no_allowed_format_throws                 | cvk logic: lifecycle, state or arguments    |
| S | capabilities                             | real surface query (shared present context) |
| F | move_assignment_destroys_the_old_surface | cvk logic: lifecycle, state or arguments    |

## surface/swapchain_test.cpp

|   | test                                       | reason                                           |
|---|--------------------------------------------|--------------------------------------------------|
| N | eval_extent                                | no Vulkan                                        |
| N | eval_image_count                           | no Vulkan                                        |
| F | base_constructor_does_not_create           | cvk logic: lifecycle, state or arguments         |
| S | create_and_destroy                         | real swapchain (shared present context)          |
| S | acquire_clear_present                      | real swapchain, layers check acquire and present |
| F | resize_retires_the_old_swapchain           | fake present fences                              |
| S | unacquire_gives_the_image_back             | real swapchain                                   |
| S | resize_unacquires_the_acquired_images      | real swapchain                                   |
| F | release_and_wrap                           | cvk logic: lifecycle, state or arguments         |
| F | move_assignment_destroys_the_old_swapchain | cvk logic: lifecycle, state or arguments         |

## utility/device/device_features_test.cpp

|   | test                               | reason    |
|---|------------------------------------|-----------|
| N | default_is_false                   | no Vulkan |
| N | bits_follow_the_field_order        | no Vulkan |
| N | merge_enables_the_features_of_both | no Vulkan |
| N | missing_names_the_features         | no Vulkan |
| N | nothing_misses_in_itself           | no Vulkan |

## utility/device/feature_chain_test.cpp

|   | test                                      | reason    |
|---|-------------------------------------------|-----------|
| N | without_extensions_links_the_core_structs | no Vulkan |
| N | links_the_structs_of_the_extensions       | no Vulkan |
| N | is_not_copyable_or_movable                | no Vulkan |
| N | bits_go_into_their_vulkan_member          | no Vulkan |
| N | features_round_trip                       | no Vulkan |
| N | extensions_of_the_features                | no Vulkan |

## utility/handle/unique_handle_test.cpp

|   | test                                    | reason                                   |
|---|-----------------------------------------|------------------------------------------|
| N | owner_names_its_handle                  | no Vulkan                                |
| N | a_handle_needs_its_context              | no Vulkan                                |
| N | constexpr                               | no Vulkan                                |
| N | size                                    | no Vulkan                                |
| F | core_deleter                            | cvk logic: lifecycle, state or arguments |
| F | move_assignment_destroys_the_old_handle | cvk logic: lifecycle, state or arguments |
| F | release_keeps_the_handle                | cvk logic: lifecycle, state or arguments |
| F | instance_deleter                        | cvk logic: lifecycle, state or arguments |
| N | handle_deleter                          | no Vulkan                                |
| F | custom_deleter                          | cvk logic: lifecycle, state or arguments |
| N | non_pointer_handle                      | no Vulkan                                |
