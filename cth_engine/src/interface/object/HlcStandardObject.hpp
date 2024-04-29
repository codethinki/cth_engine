#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>



namespace cth {
class InputController;



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
    StandardObject() { _id = currentId++; };
    explicit StandardObject(const Transform& transform) : _transform{transform} { _id = currentId++; };
    virtual ~StandardObject() = 0;

    [[nodiscard]] uint32_t getId() const { return _id; }

    StandardObject(const StandardObject&) = default;
    StandardObject& operator=(const StandardObject&) = default;
    StandardObject(StandardObject&&) = default;
    StandardObject& operator=(StandardObject&&) = default; // copy/move operations


protected:
    Transform _transform{};
    uint32_t _id;
    inline static uint32_t currentId{};

private:
    friend InputController;
};

inline StandardObject::~StandardObject() {}

}
