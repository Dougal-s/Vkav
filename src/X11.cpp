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
			val = XInternAtom(glfwGetX11Display(), "_NET_WM_WINDOW_TYPE_NORMAL", False);
			break;
		case WindowType::DESKTOP:
			val = XInternAtom(glfwGetX11Display(), "_NET_WM_WINDOW_TYPE_DESKTOP", False);
			break;
	}

	XChangeProperty(glfwGetX11Display(), glfwGetX11Window(window),
	                XInternAtom(glfwGetX11Display(), "_NET_WM_WINDOW_TYPE", False), XA_ATOM, 32,
	                PropModeReplace, reinterpret_cast<unsigned char*>(&val), 1);
}

void setSticky(GLFWwindow* window) {
	Atom wmStateSticky = XInternAtom(glfwGetX11Display(), "_NET_WM_STATE_STICKY", False);
	Atom wmState = XInternAtom(glfwGetX11Display(), "_NET_WM_STATE", False);

	XClientMessageEvent msgEvent = {};
	msgEvent.type = ClientMessage;
	msgEvent.window = glfwGetX11Window(window);
	msgEvent.message_type = wmState;
	msgEvent.format = 32;
	msgEvent.data.l[0] = 1;
	msgEvent.data.l[1] = wmStateSticky;
	msgEvent.data.l[2] = 0;
	msgEvent.data.l[3] = 1;

	XSendEvent(glfwGetX11Display(), DefaultRootWindow(glfwGetX11Display()), false,
	           SubstructureRedirectMask, reinterpret_cast<XEvent*>(&msgEvent));
}
#endif
