#include "HlcInputController.hpp"

#include "../object/HlcStandardObject.hpp"


namespace cth::vk {
void InputController::moveByKeys(const float dt, const std::unique_ptr<StandardObject>& object) const {
    const float yaw = object->_transform.rotation.y;
    const glm::vec3 forwardDir{sin(yaw), 0.f, cos(yaw)};
    const glm::vec3 rightDir{forwardDir.z, 0.f, -forwardDir.x};
    constexpr glm::vec3 upDir{0.f, -1.f, 0.f};

    glm::vec3 moveDir{0.f};
    if(getKeyState(MOVE_FORWARD)) moveDir += forwardDir;
    if(getKeyState(MOVE_BACKWARD)) moveDir -= forwardDir;
    if(getKeyState(MOVE_RIGHT)) moveDir += rightDir;
    if(getKeyState(MOVE_LEFT)) moveDir -= rightDir;
    if(getKeyState(MOVE_UP)) moveDir += upDir;
    if(getKeyState(MOVE_DOWN)) moveDir -= upDir;
    if(dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) object->_transform.translation += moveSpeed * dt * normalize(moveDir);
}
void InputController::rotateByMouse(const float dt, const std::unique_ptr<StandardObject>& object) const {
    object->_transform.rotation += glm::vec3(-mouseDtY, mouseDtX, 0) * dt * 0.2f;
    object->_transform.rotation.x = glm::clamp(object->_transform.rotation.x, -1.5f, 1.5f);
    object->_transform.rotation.y = glm::mod(object->_transform.rotation.y, glm::two_pi<float>());
}

static constexpr int MOUSE_SMOOTH = 4;
static std::array<double, MOUSE_SMOOTH> mouseDtsX{};
static std::array<double, MOUSE_SMOOTH> mouseDtsY{};
static double prevMouseX = 0, prevMouseY = 0, mouseDtsXSum = 0, mouseDtsYSum = 0;

void InputController::updateMousePos(GLFWwindow* window) {
    static uint_fast8_t mouseDtArrPos = 0;


    ++mouseDtArrPos %= MOUSE_SMOOTH;

    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    const double dtX = mouseX - prevMouseX, dtY = mouseY - prevMouseY;
    prevMouseX = mouseX;
    prevMouseY = mouseY;

    mouseDtsXSum += dtX - mouseDtsX[mouseDtArrPos];
    mouseDtsYSum += dtY - mouseDtsY[mouseDtArrPos];

    mouseDtsX[mouseDtArrPos] = dtX;
    mouseDtsY[mouseDtArrPos] = dtY;

    mouseDtX = mouseDtsXSum / static_cast<double>(MOUSE_SMOOTH);
    mouseDtY = mouseDtsYSum / static_cast<double>(MOUSE_SMOOTH);
}
void InputController::resetMouseDt(GLFWwindow* window) {
    glfwGetCursorPos(window, &prevMouseX, &prevMouseY);
    mouseDtsX.fill(0.0);
    mouseDtsY.fill(0.0);

    mouseDtsXSum = 0;
    mouseDtsYSum = 0;

    mouseDtX = 0;
    mouseDtY = 0;
}
}
