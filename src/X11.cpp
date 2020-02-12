#ifdef X11
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xatom.h>

#include "NativeWindowHints.hpp"

void setWindowType(GLFWwindow* window, WindowType type) {
	long val;
	switch (type) {
		case WindowType::NORMAL:
		val = XInternAtom (glfwGetX11Display(), "_NET_WM_WINDOW_TYPE_NORMAL", False);
		break;
		case WindowType::DESKTOP:
		val = XInternAtom (glfwGetX11Display(), "_NET_WM_WINDOW_TYPE_DESKTOP", False);
		break;
	}

	XChangeProperty (
		glfwGetX11Display(),
		glfwGetX11Window(window),
		XInternAtom (glfwGetX11Display(), "_NET_WM_WINDOW_TYPE", False),
		XA_ATOM,
		32,
		PropModeReplace,
		reinterpret_cast<unsigned char *>(&val),
		1
	);
}
#endif
