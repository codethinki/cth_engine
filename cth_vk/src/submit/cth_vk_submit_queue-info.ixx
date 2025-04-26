export module cth.vk.submit.queue_info;

import cth.vk.util.types;
import cth.vk.submit.queue_family;

export namespace cth::vk {
struct QueueInfo {
    QueueFamilyProperties familyProperties;
    uint32_t familyIndex;
    /**
     * @brief index in the family
     */
    uint32_t queueIndex;
    vk::not_null<VkQueue> vkQueue;
};
}