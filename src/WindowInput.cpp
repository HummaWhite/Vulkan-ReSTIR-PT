#include "WindowInput.h"
#include <set>

namespace WindowInput {
	constexpr float CameraRotateSensitivity = 0.2f;
	constexpr float CameraMoveSensitivity = 0.005f;
	constexpr float CameraRollSensitivity = 0.002f;
	constexpr float CamerFOVSensitivity = 150.0f;

	Camera* camera = nullptr;
	double lastCursorX = 0;
	double lastCursorY = 0;
	bool firstCursorMove = true;
	bool cursorDisabled = true;
	bool locked = false;
	double deltaTime = 0;
	std::set<int> pressedKeys;

	void setCamera(Camera& camera) {
		WindowInput::camera = &camera;
	}

	void setDeltaTime(double delta) {
		deltaTime = delta;
	}

	bool isPressingKey(int keyCode) {
		return pressedKeys.find(keyCode) != pressedKeys.end();
	}

	void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mode) {
		if (action == GLFW_PRESS) {
			pressedKeys.insert(key);
		}
		else if (action == GLFW_RELEASE) {
			pressedKeys.erase(key);
		}

		if (action == GLFW_PRESS){
			if (key == GLFW_KEY_ESCAPE) {
				glfwSetWindowShouldClose(window, true);
			}
			else if (key == GLFW_KEY_F1) {
				if (cursorDisabled) {
					firstCursorMove = true;
				}
				cursorDisabled ^= 1;
				glfwSetInputMode(window, GLFW_CURSOR, cursorDisabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
			}
			else if (key == GLFW_KEY_F2) {
				locked ^= 1;
			}
			if (!isPressingKey(GLFW_KEY_LEFT_CONTROL) && !isPressingKey(GLFW_KEY_RIGHT_CONTROL)) {
				return;
			}
		}
	}

	void cursorCallback(GLFWwindow* window, double posX, double posY) {
		if (cursorDisabled || locked) {
			return;
		}

		if (firstCursorMove) {
			lastCursorX = posX;
			lastCursorY = posY;
			firstCursorMove = false;
			return;
		}

		float offsetX = static_cast<float>(posX - lastCursorX);
		float offsetY = static_cast<float>(posY - lastCursorY);
		glm::vec3 offset(offsetX, -offsetY, 0);

		float speed = isPressingKey(GLFW_KEY_LEFT_CONTROL) ? .1f : 1.f;

		camera->rotate(offset * CameraRotateSensitivity * speed);

		lastCursorX = posX;
		lastCursorY = posY;
	}

	void scrollCallback(GLFWwindow* window, double offsetX, double offsetY) {
		if (cursorDisabled || locked) {
			return;
		}
		float speed = isPressingKey(GLFW_KEY_LEFT_CONTROL) ? .1f : 1.f;
		camera->changeFOV(CamerFOVSensitivity * speed * static_cast<float>(offsetY));
	}

	void moveCamera(int key) {
		if (locked) {
			return;
		}

		glm::vec3 angle = camera->angle();
		glm::vec3 dPos(0.f);
		float dRoll = 0.f;

		switch (key) {
		case GLFW_KEY_W:
			dPos = { glm::sin(glm::radians(angle.x)), glm::cos(glm::radians(angle.x)), 0.f };
			break;
		case GLFW_KEY_S:
			dPos = { -glm::sin(glm::radians(angle.x)), -glm::cos(glm::radians(angle.x)), 0.f };
			break;
		case GLFW_KEY_A:
			dPos = { -glm::cos(glm::radians(angle.x)), glm::sin(glm::radians(angle.x)), 0.f };
			break;
		case GLFW_KEY_D:
			dPos = { glm::cos(glm::radians(angle.x)), -glm::sin(glm::radians(angle.x)), 0.f };
			break;
		case GLFW_KEY_SPACE:
			dPos.z = 1.f;
			break;
		case GLFW_KEY_LEFT_SHIFT:
			dPos.z = -1.f;
			break;
		case GLFW_KEY_Q:
			dRoll = -1.f;
			break;
		case GLFW_KEY_E:
			dRoll = 1.f;
			break;
		case GLFW_KEY_R:
			camera->setAngle({ angle.x, angle.y, 0.f });
			break;
		}
		float speed = isPressingKey(GLFW_KEY_LEFT_CONTROL) ? .1f : 1.f;

		camera->move(dPos * CameraMoveSensitivity * speed * static_cast<float>(deltaTime));
		camera->roll(dRoll * CameraRollSensitivity * speed * static_cast<float>(deltaTime));
	}

	void processKeys()
	{
		const int keys[] = {
			GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_A, GLFW_KEY_D,
			GLFW_KEY_Q, GLFW_KEY_E, GLFW_KEY_R, GLFW_KEY_LEFT_SHIFT, GLFW_KEY_SPACE
		};

		for (auto key : keys) {
			if (isPressingKey(key)) {
				moveCamera(key);
			}
		}
	}
};