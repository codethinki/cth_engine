#include "HlcCamera.hpp"

#include <cassert>
#include <limits>


namespace cth {
void Camera::setOrthographicProjection(const float left, const float right, const float top, const float bottom, const float near, const float far) {
	projectionMatrix = glm::mat4{1.0f};
	projectionMatrix[0][0] = 2.f / (right - left);
	projectionMatrix[1][1] = 2.f / (bottom - top);
	projectionMatrix[2][2] = 1.f / (far - near);
	projectionMatrix[3][0] = -(right + left) / (right - left);
	projectionMatrix[3][1] = -(bottom + top) / (bottom - top);
	projectionMatrix[3][2] = -near / (far - near);
}

void Camera::setPerspectiveProjection(const float fov, const float view_ratio, const float near_clip, const float far_clip) {
	assert(glm::abs(view_ratio - std::numeric_limits<float>::epsilon()) > 0.0f);

	currentProjection = glm::vec3{fov, near_clip, far_clip};

	const float tanHalfFov = tan(fov / 2.f);
	projectionMatrix = glm::mat4{0.0f};
	projectionMatrix[0][0] = 1.f / (tanHalfFov * view_ratio);
	projectionMatrix[1][1] = 1.f / (tanHalfFov);
	projectionMatrix[2][2] = far_clip / (far_clip - near_clip);
	projectionMatrix[2][3] = 1.f;
	projectionMatrix[3][2] = -(far_clip * near_clip) / (far_clip - near_clip);
}
void Camera::correctViewRatio(const float view_ratio) {
	setPerspectiveProjection(currentProjection.x, view_ratio, currentProjection.y, currentProjection.z);
}

void Camera::setViewDirection(const glm::vec3 position, const glm::vec3 direction, const glm::vec3 up) {
	const glm::vec3 w{normalize(direction)};
	const glm::vec3 u{normalize(cross(w, up))};
	const glm::vec3 v{cross(w, u)};

	viewMatrix = glm::mat4{1.f};
	viewMatrix[0][0] = u.x;
	viewMatrix[1][0] = u.y;
	viewMatrix[2][0] = u.z;
	viewMatrix[0][1] = v.x;
	viewMatrix[1][1] = v.y;
	viewMatrix[2][1] = v.z;
	viewMatrix[0][2] = w.x;
	viewMatrix[1][2] = w.y;
	viewMatrix[2][2] = w.z;
	viewMatrix[3][0] = -dot(u, position);
	viewMatrix[3][1] = -dot(v, position);
	viewMatrix[3][2] = -dot(w, position);
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
	viewMatrix = glm::mat4{1.f};
	viewMatrix[0][0] = u.x;
	viewMatrix[1][0] = u.y;
	viewMatrix[2][0] = u.z;
	viewMatrix[0][1] = v.x;
	viewMatrix[1][1] = v.y;
	viewMatrix[2][1] = v.z;
	viewMatrix[0][2] = w.x;
	viewMatrix[1][2] = w.y;
	viewMatrix[2][2] = w.z;
	viewMatrix[3][0] = -dot(u, position);
	viewMatrix[3][1] = -dot(v, position);
	viewMatrix[3][2] = -dot(w, position);
}
}