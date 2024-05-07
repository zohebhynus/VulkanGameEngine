#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>

class Window
{
public:
	Window(int width, int height, std::string name);
	~Window();

	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;

	bool ShouldClose() { return glfwWindowShouldClose(m_Window); }
	VkExtent2D GetExtent() { return { static_cast<uint32_t>(m_WindowWidth), static_cast<uint32_t>(m_WindowHeight) }; }
	void CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface);

	bool WasWindowResized() { return m_FrameBufferResized; }
	void ResetWindowResizedFlag() { m_FrameBufferResized = false; }

	GLFWwindow* GetWindow() const { return m_Window; }

private:
	static void frameBufferResizeCallback(GLFWwindow* glfwWindow, int width, int height);
	GLFWwindow* m_Window;

	int m_WindowWidth;
	int m_WindowHeight;
	bool m_FrameBufferResized = false;

	std::string m_WindowName;
};
