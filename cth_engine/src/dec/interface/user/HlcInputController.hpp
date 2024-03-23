#pragma once
#include <array>
#include <memory>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

struct GLFWwindow;


namespace cth {
using namespace std;

class StandardObject;

class InputController {
public:
		enum Key_Mappings {
		MOVE_FORWARD,
		MOVE_BACKWARD,
		MOVE_LEFT,
		MOVE_RIGHT,
		MOVE_UP,
		MOVE_DOWN,
		PIECE_ROTATE_LEFT,
		PIECE_ROTATE_RIGHT,
		PIECE_PLACE,
		PIECE_MOVE_LEFT,
		PIECE_MOVE_RIGHT,
		KEY_MAPPINGS_SIZE
	};

	void moveByKeys(float dt, const unique_ptr<StandardObject>& object) const;

	void rotateByMouse(float dt, const unique_ptr<StandardObject>& object) const;

	explicit InputController() = default;
	~InputController() = default;

	static void updateMousePos(GLFWwindow* window);
	static void resetMouseDt(GLFWwindow* window);


	static bool getKeyState(const Key_Mappings key) {return keyStates[KEY_MAPPINGS[key]];}

	static constexpr auto KEY_MAPPINGS = [] {
		array<int, KEY_MAPPINGS_SIZE> arr{};

		arr[MOVE_FORWARD] = GLFW_KEY_W;
		arr[MOVE_BACKWARD] = GLFW_KEY_S;
		arr[MOVE_LEFT] = GLFW_KEY_A;
		arr[MOVE_RIGHT] = GLFW_KEY_D;
		arr[MOVE_DOWN] = GLFW_KEY_LEFT_CONTROL;
		arr[MOVE_UP] = GLFW_KEY_LEFT_SHIFT;
		arr[PIECE_MOVE_LEFT] = GLFW_KEY_LEFT;
		arr[PIECE_MOVE_RIGHT] = GLFW_KEY_RIGHT;
		arr[PIECE_PLACE] = GLFW_KEY_RIGHT_CONTROL;
		arr[PIECE_ROTATE_RIGHT] = GLFW_KEY_UP;
		arr[PIECE_ROTATE_LEFT] = GLFW_KEY_DOWN;

		return arr;
	}();

	inline static float moveSpeed{2.f};
	inline static float cameraSensitivity{2.f};

	inline static double mouseDtX = 0;
	inline static double mouseDtY = 0;

	inline static array<bool, 349>keyStates{};
};
}
