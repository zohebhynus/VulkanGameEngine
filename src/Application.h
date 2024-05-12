#pragma once
#include "Window.h"
#include "Graphics/Renderer.h"
#include "Graphics/Descriptor.h"
#include "Model.h"

#include <memory>
#include <vector>

class Application
{
public:
	static constexpr int WIDTH = 800;
	static constexpr int HEIGHT = 600;

	Application();
	~Application();

	Application(const Application&) = delete;
	Application operator=(const Application&) = delete;

	void Run();

private:
	void LoadGameObjects();


	Window m_AppWindow {WIDTH, HEIGHT, "App Window"};
	Device m_Device{ m_AppWindow };
	Renderer m_Renderer{ m_AppWindow, m_Device };

	std::unique_ptr<DescriptorPool> m_GlobalPool{};
	std::vector<VkDescriptorSetLayout> m_SetLayouts;
	std::vector<std::shared_ptr<Model>> m_Models;

	glm::vec3 lightDir {-30.0f, 30.0f, 10.0f};
};