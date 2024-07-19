#include "HlcCamera.hpp"


namespace cth::vk {
void Camera::setOrthographicProjection(const float left, const float right, const float top, const float bottom, const float near, const float far) {
    _projectionMatrix = glm::mat4{1.0f};
    _projectionMatrix[0][0] = 2.f / (right - left);
    _projectionMatrix[1][1] = 2.f / (bottom - top);
    _projectionMatrix[2][2] = 1.f / (far - near);
    _projectionMatrix[3][0] = -(right + left) / (right - left);
    _projectionMatrix[3][1] = -(bottom + top) / (bottom - top);
    _projectionMatrix[3][2] = -near / (far - near);
}

void Camera::setPerspectiveProjection(const float fov, const float view_ratio, const float near_clip, const float far_clip) {
    CTH_ASSERT(glm::abs(view_ratio - std::numeric_limits<float>::epsilon()) > 0.0f, "must be > 0") {}

    _currentProjection = glm::vec3{fov, near_clip, far_clip};

    const float tanHalfFov = tan(fov / 2.f);
    _projectionMatrix = glm::mat4{0.0f};
    _projectionMatrix[0][0] = 1.f / (tanHalfFov * view_ratio);
    _projectionMatrix[1][1] = 1.f / (tanHalfFov);
    _projectionMatrix[2][2] = far_clip / (far_clip - near_clip);
    _projectionMatrix[2][3] = 1.f;
    _projectionMatrix[3][2] = -(far_clip * near_clip) / (far_clip - near_clip);
}
void Camera::correctViewRatio(const float view_ratio) {
    setPerspectiveProjection(_currentProjection.x, view_ratio, _currentProjection.y, _currentProjection.z);
}

void Camera::setViewDirection(const glm::vec3 position, const glm::vec3 direction, const glm::vec3 up) {
    const glm::vec3 w{normalize(direction)};
    const glm::vec3 u{normalize(cross(w, up))};
    const glm::vec3 v{cross(w, u)};

    _viewMatrix = glm::mat4{1.f};
    _viewMatrix[0][0] = u.x;
    _viewMatrix[1][0] = u.y;
    _viewMatrix[2][0] = u.z;
    _viewMatrix[0][1] = v.x;
    _viewMatrix[1][1] = v.y;
    _viewMatrix[2][1] = v.z;
    _viewMatrix[0][2] = w.x;
    _viewMatrix[1][2] = w.y;
    _viewMatrix[2][2] = w.z;
    _viewMatrix[3][0] = -dot(u, position);
    _viewMatrix[3][1] = -dot(v, position);
    _viewMatrix[3][2] = -dot(w, position);
}

void Camera::setViewTarget(const glm::vec3 position, const glm::vec3 target, const glm::vec3 up) {
    setViewDirection(position, target - position, up);
}

void Camera::setViewYXZ(const glm::vec3 position, const glm::vec3 rotation) {
    const float c3 = glm::cos(rotation.z), s3 = glm::sin(rotation.z), c2 = glm::cos(rotation.x), s2 =
                    glm::sin(rotation.x), c1 = glm::cos(rotation.y), s1 = glm::sin(rotation.y);

    const glm::vec3 u{(c1 * c3 + s1 * s2 * s3), (c2 * s3), (c1 * s2 * s3 - c3 * s1)},
                    v{(c3 * s1 * s2 - c1 * s3), (c2 * c3), (c1 * c3 * s2 + s1 * s3)},
                    w{(c2 * s1), (-s2), (c1 * c2)};
    _viewMatrix = glm::mat4{1.f};
    _viewMatrix[0][0] = u.x;
    _viewMatrix[1][0] = u.y;
    _viewMatrix[2][0] = u.z;
    _viewMatrix[0][1] = v.x;
    _viewMatrix[1][1] = v.y;
    _viewMatrix[2][1] = v.z;
    _viewMatrix[0][2] = w.x;
    _viewMatrix[1][2] = w.y;
    _viewMatrix[2][2] = w.z;
    _viewMatrix[3][0] = -dot(u, position);
    _viewMatrix[3][1] = -dot(v, position);
    _viewMatrix[3][2] = -dot(w, position);
}
}
