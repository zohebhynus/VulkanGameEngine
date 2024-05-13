#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm/glm.hpp>

#include "../Components.h"

class CameraSystem : public System
{
public:

	struct KeyMappings {
		int moveLeft = GLFW_KEY_A;
		int moveRight = GLFW_KEY_D;
		int moveForward = GLFW_KEY_W;
		int moveBackward = GLFW_KEY_S;
		int moveUp = GLFW_KEY_E;
		int moveDown = GLFW_KEY_Q;
		int lookLeft = GLFW_KEY_LEFT;
		int lookRight = GLFW_KEY_RIGHT;
		int lookUp = GLFW_KEY_UP;
		int lookDown = GLFW_KEY_DOWN;
	};
	void SetViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = glm::vec3(0.0f, -1.0f, 0.0f));

	void SetOrthographicProjectionEditorCam(float left, float right, float top, float bottom, float near, float far);
	void SetPerspectiveProjectionEditorCam(float fovy, float aspect, float near, float far);

	//void SetNearAndFar(float near, float far);

	void UpdateEditorCameraTransform(glm::vec3 position, glm::vec3 rotation);

	void SetEditorCamera(bool active = true) { m_IsEditorActive = active; }

	void EditorCameraInput(GLFWwindow* window, float dt);

	const glm::vec3& GetEditorCameraPosition() const { return m_Position; }
	const glm::mat4& GetProjection() const { return m_ProjectionMatrix; }
	const glm::mat4& GetView() const { return m_ViewMatrix; }
	const glm::mat4& GetInverseView() const { return m_InverseViewMatrix; }
	const float GetNear() const { return m_Near; }
	const float GetFar() const { return m_Far; }
private:
	void SetViewYXZ(glm::vec3 position, glm::vec3 rotation);
	void SetViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3(0.0f, -1.0f, 0.0f));

	glm::vec3 m_Position{ 1.0f };
	glm::vec3 m_Rotation{ 1.0f };

	glm::mat4 m_ProjectionMatrix{ 1.0f };
	glm::mat4 m_ViewMatrix{ 1.0f };
	glm::mat4 m_InverseViewMatrix{ 1.0f };
	bool m_IsEditorActive = true;

	KeyMappings keys{};
	float m_MoveSpeed{ 5.0f };
	float m_LookSpeed{ 2.0f };

	float m_Near = 0.1f;
	float m_Far  = 100.0f;
};