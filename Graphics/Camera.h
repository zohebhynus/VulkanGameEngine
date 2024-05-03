#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm/glm.hpp>

class Camera
{
public:


	void SetOrthographicProjection(float left, float right, float top, float bottom, float near, float far);
	void SetPerspectiveProjection(float fovy, float aspect, float near, float far);

	void SetViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3(0.0f, -1.0f, 0.0f));
	void SetViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = glm::vec3(0.0f, -1.0f, 0.0f));
	void SetViewYXZ(glm::vec3 position, glm::vec3 rotation);

	const glm::mat4& GetProjection() const { return m_ProjectionMatrix; }
	const glm::mat4& GetView() const { return m_ViewMatrix; }
	const glm::mat4& GetInverseView() const { return m_InverseViewMatrix; }
	const glm::vec3 getPosition() const { return glm::vec3(m_InverseViewMatrix[3]); }


private:
	glm::mat4 m_ProjectionMatrix{ 1.0f };
	glm::mat4 m_ViewMatrix {1.0f};
	glm::mat4 m_InverseViewMatrix{ 1.0f };
};