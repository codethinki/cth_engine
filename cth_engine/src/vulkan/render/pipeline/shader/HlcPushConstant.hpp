#pragma once
#include <glm/glm.hpp>



namespace cth::vk {

struct PushConstants {
    glm::mat4 matrix;
    glm::vec4 color;
    void set(glm::mat4 const& new_matrix, glm::vec3 const& new_color) {
        matrix = new_matrix;
        color = glm::vec4(new_color, 0.f);
    }
    PushConstants() = default;
    explicit PushConstants(glm::vec3 const& color, glm::mat4 const& matrix): matrix{matrix}, color{glm::vec4(color, 0.f)} {}

    inline static constexpr uint32_t MATRIX_SIZE = sizeof(matrix);
    inline static constexpr uint32_t MATRIX_OFFSET = 0;

    inline static constexpr uint32_t COLOR_SIZE = sizeof(PushConstants::color);
    inline static constexpr uint32_t COLOR_OFFSET = MATRIX_SIZE;
};


namespace push_info {
constexpr uint32_t RANGE_COUNT = 1;
constexpr VkPushConstantRange RANGE_INFO{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants)};
}

}
