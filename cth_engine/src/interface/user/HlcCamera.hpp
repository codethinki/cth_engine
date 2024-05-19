#pragma once

#include <glm/glm.hpp>

namespace cth {

class Camera {
public:
    void setOrthographicProjection(float left, float right, float top, float bottom, float near, float far);
    void setPerspectiveProjection(float fov, float view_ratio, float near_clip, float far_clip);

    void correctViewRatio(float view_ratio);

    void setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3(0.f, -1.f, 0.f));
    void setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = glm::vec3(0.f, -1.f, 0.f));
    void setViewYXZ(glm::vec3 position, glm::vec3 rotation);



    [[nodiscard]] const glm::mat4& getProjection() const { return _projectionMatrix; }
    [[nodiscard]] const glm::mat4& getView() const { return _viewMatrix; }

private:
    glm::mat4 _projectionMatrix{1.f};
    glm::mat4 _viewMatrix{1.f};

    glm::vec3 _currentProjection{};

};
}
