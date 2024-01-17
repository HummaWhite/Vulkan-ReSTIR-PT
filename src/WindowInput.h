#pragma once

#include <GLFW/glfw3.h>
#include "Camera.h"

namespace WindowInput {
	void setCamera(Camera& camera);
	void setDeltaTime(double delta);
	bool isPressingKey(int keyCode);
	bool displayGUI();
	bool shouldCaptureScreen();
	bool setShouldCaptureScreen(bool should);
	void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
	void cursorCallback(GLFWwindow* window, double posX, double posY);
	void scrollCallback(GLFWwindow* window, double offsetX, double offsetY);
	void processKeys();
};