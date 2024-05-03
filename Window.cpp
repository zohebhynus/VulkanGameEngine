#include "Window.h"

#include <stdexcept>

Window::Window(int width, int height, std::string name) :
	m_WindowWidth(width),
	m_WindowHeight(height),
	m_WindowName(name)
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	m_Window = glfwCreateWindow(m_WindowWidth, m_WindowHeight, m_WindowName.c_str(), nullptr, nullptr);
	glfwSetWindowUserPointer(m_Window, this);
	glfwSetFramebufferSizeCallback(m_Window, frameBufferResizeCallback);
}

Window::~Window()
{
	glfwDestroyWindow(m_Window);
	glfwTerminate();
}

void Window::CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
{
	if (glfwCreateWindowSurface(instance, m_Window, nullptr, surface) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create window surface");
	}
}

void Window::frameBufferResizeCallback(GLFWwindow* glfwWindow, int width, int height)
{
	auto window = reinterpret_cast<Window *>(glfwGetWindowUserPointer(glfwWindow));

	window->m_WindowHeight = height;
	window->m_WindowWidth = width;
	window->m_FrameBufferResized = true;
}
