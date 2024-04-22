#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


namespace cth {



struct Transform {
    explicit Transform(const glm::vec3& translation = {}, const glm::vec3& scale = {}, const glm::vec3& rotation = {}) : translation(translation),
        scale(scale),
        rotation(rotation) {}

    glm::vec3 translation{};
    glm::vec3 scale{1.f, 1.f, 1.f};
    glm::vec3 rotation{}; // Y Z X Tait-Bryan
    [[nodiscard]] glm::mat4 matrix() const;
    [[nodiscard]] glm::mat3 normal() const;
};

class StandardObject {
public:
    StandardObject() { id = currentId++; };
    explicit StandardObject(const Transform& transform) : transform{transform} { id = currentId++; };
    virtual ~StandardObject() = 0;

    [[nodiscard]] uint32_t getId() const { return id; }

    StandardObject(const StandardObject&) = default;
    StandardObject& operator=(const StandardObject&) = default;
    StandardObject(StandardObject&&) = default;
    StandardObject& operator=(StandardObject&&) = default; // copy/move operations

    Transform transform{};

protected:
    uint32_t id;
    inline static uint32_t currentId{};
};

inline StandardObject::~StandardObject() {}

}
