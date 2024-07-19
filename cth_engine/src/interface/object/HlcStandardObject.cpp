#include "HlcStandardObject.hpp"

namespace cth::vk {
    //TODO this is untested but could work idk
//glm::mat4 Transform::matrix(const glm::vec3& direction_scale_normal) const {
//    //assuming your model has the base (center of the bottom circle) at (0, 0, 0) and is 1 in height
//    const glm::vec3& x = direction_scale_normal; //a and b are bottom to top vectors
//    glm::vec3 u = glm::vec3(0.0f, -1.0f, 0.0f); //assuming u have a negative y axis
//    glm::vec3 v = glm::cross(x, u);
//
//    //rotate and scale based on x
//    return glm::mat4{
//        // Assign rotation based on x, v, and u
//        glm::vec4(x, 0.f),
//        glm::vec4(v, 0.f),
//        glm::vec4(u, 0.f),
//        glm::vec4(translation.x, translation.y, translation.z, 1.f) //translation
//    };
//
//}

glm::mat4 Transform::matrix() const {
    const float c3 = glm::cos(rotation.z), s3 = glm::sin(rotation.z), c2 = glm::cos(rotation.x), s2 = glm::sin(
                    rotation.x), c1 = glm::cos(rotation.y), s1 = glm::sin(rotation.y);

    return glm::mat4{
        {scale.x * (c1 * c3 + s1 * s2 * s3),
            scale.x * (c2 * s3),
            scale.x * (c1 * s2 * s3 - c3 * s1),
            0.0f},
        {scale.y * (c3 * s1 * s2 - c1 * s3),
            scale.y * (c2 * c3),
            scale.y * (c1 * c3 * s2 + s1 * s3),
            0.0f},
        {scale.z * (c2 * s1),
            scale.z * -s2,
            scale.z * (c1 * c2),
            0.0f},
        {translation.x, translation.y, translation.z, 1.0f}};
}
glm::mat3 Transform::normal() const {
    const float c3 = glm::cos(rotation.z), s3 = glm::sin(rotation.z), c2 = glm::cos(rotation.x), s2 = glm::sin(
                    rotation.x), c1 = glm::cos(rotation.y), s1 = glm::sin(rotation.y);

    const glm::vec3 invScale = 1.0f / scale;

    return glm::mat3{
        {invScale.x * (c1 * c3 + s1 * s2 * s3),
            invScale.x * (c2 * s3),
            invScale.x * (c1 * s2 * s3 - c3 * s1)},
        {invScale.y * (c3 * s1 * s2 - c1 * s3),
            invScale.y * (c2 * c3),
            invScale.y * (c1 * c3 * s2 + s1 * s3)},
        {invScale.z * (c2 * s1),
            invScale.z * -s2,
            invScale.z * (c1 * c2)}};
}

}
