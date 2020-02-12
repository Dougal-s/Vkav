#pragma once
#ifdef NATIVE_WINDOW_HINTS_SUPPORTED
struct GLFWwindow;

enum WindowType { NORMAL, DESKTOP };

void setWindowType(GLFWwindow* window, WindowType type);
#endif
