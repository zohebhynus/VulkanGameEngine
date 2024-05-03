#pragma once

#include "ModelTwo.h"

#include <glm/glm/gtc/matrix_transform.hpp>

#include <memory>
#include <unordered_map>

struct TransformComponent
{
	glm::vec3 translation {};
	glm::vec3 scale { 1.0f, 1.0f , 1.0f};
	glm::vec3 rotation {};

	// Matrix corrsponds to Translate * Ry * Rx * Rz * Scale
	// Rotations correspond to Tait-bryan angles of Y(1), X(2), Z(3)
	// https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
	glm::mat4 mat4();
	glm::mat3 normalMatrix();
};

struct PointLightComponent
{
	float lightIntensity = 1.0f;
};

class GameObject
{
public:

	static GameObject CreateGameObject()
	{
		static unsigned int id = 0;
		return GameObject(id++);
	}

	static GameObject MakePointLight(float intensity = 10.0f, float radius = 0.1f, glm::vec3 color = glm::vec3(1.0f));

	GameObject(const GameObject&) = delete;
	GameObject& operator=(const GameObject&) = delete;
	GameObject(GameObject&&) = default;
	GameObject& operator=(GameObject&&) = default;

	unsigned int GetID()
	{
		return m_ID;
	}

	std::shared_ptr<Model> m_Model {};
	glm::vec3 m_Color{};
	TransformComponent m_Transform {};

	std::unique_ptr<PointLightComponent> m_PointLight = nullptr;

private:

	GameObject(unsigned int id) : m_ID(id) {}

	unsigned int m_ID;
};