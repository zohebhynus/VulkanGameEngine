#include "CameraSystem.h"

void CameraSystem::SetOrthographicProjectionEditorCam(float left, float right, float top, float bottom, float near, float far)
{
	m_ProjectionMatrix = glm::mat4{ 1.0f };
	m_ProjectionMatrix[0][0] = 2.f / (right - left);
	m_ProjectionMatrix[1][1] = 2.f / (bottom - top);
	m_ProjectionMatrix[2][2] = 1.f / (far - near);
	m_ProjectionMatrix[3][0] = -(right + left) / (right - left);
	m_ProjectionMatrix[3][1] = -(bottom + top) / (bottom - top);
	m_ProjectionMatrix[3][2] = -near / (far - near);
}

void CameraSystem::SetPerspectiveProjectionEditorCam(float fovy, float aspect, float near, float far)
{
	assert(glm::abs(aspect - std::numeric_limits<float>::epsilon()) > 0.0f);
	const float tanHalfFovy = tan(fovy / 2.f);
	m_ProjectionMatrix = glm::mat4{ 0.0f };
	m_ProjectionMatrix[0][0] = 1.f / (aspect * tanHalfFovy);
	m_ProjectionMatrix[1][1] = 1.f / (tanHalfFovy);
	m_ProjectionMatrix[2][2] = far / (far - near);
	m_ProjectionMatrix[2][3] = 1.f;
	m_ProjectionMatrix[3][2] = -(far * near) / (far - near);
}

void CameraSystem::UpdateEditorCameraTransform(glm::vec3 position, glm::vec3 rotation)
{
	m_Position = position;
	m_Rotation = rotation;

	SetViewYXZ(m_Position, m_Rotation);
}

void CameraSystem::SetViewYXZ(glm::vec3 position, glm::vec3 rotation)
{
	const float c3 = glm::cos(rotation.z);
	const float s3 = glm::sin(rotation.z);
	const float c2 = glm::cos(rotation.x);
	const float s2 = glm::sin(rotation.x);
	const float c1 = glm::cos(rotation.y);
	const float s1 = glm::sin(rotation.y);
	const glm::vec3 u{ (c1 * c3 + s1 * s2 * s3), (c2 * s3), (c1 * s2 * s3 - c3 * s1) };
	const glm::vec3 v{ (c3 * s1 * s2 - c1 * s3), (c2 * c3), (c1 * c3 * s2 + s1 * s3) };
	const glm::vec3 w{ (c2 * s1), (-s2), (c1 * c2) };
	m_ViewMatrix = glm::mat4{ 1.f };
	m_ViewMatrix[0][0] = u.x;
	m_ViewMatrix[1][0] = u.y;
	m_ViewMatrix[2][0] = u.z;
	m_ViewMatrix[0][1] = v.x;
	m_ViewMatrix[1][1] = v.y;
	m_ViewMatrix[2][1] = v.z;
	m_ViewMatrix[0][2] = w.x;
	m_ViewMatrix[1][2] = w.y;
	m_ViewMatrix[2][2] = w.z;
	m_ViewMatrix[3][0] = -glm::dot(u, position);
	m_ViewMatrix[3][1] = -glm::dot(v, position);
	m_ViewMatrix[3][2] = -glm::dot(w, position);

	m_InverseViewMatrix = glm::mat4{ 1.f };
	m_InverseViewMatrix[0][0] = u.x;
	m_InverseViewMatrix[0][1] = u.y;
	m_InverseViewMatrix[0][2] = u.z;
	m_InverseViewMatrix[1][0] = v.x;
	m_InverseViewMatrix[1][1] = v.y;
	m_InverseViewMatrix[1][2] = v.z;
	m_InverseViewMatrix[2][0] = w.x;
	m_InverseViewMatrix[2][1] = w.y;
	m_InverseViewMatrix[2][2] = w.z;
	m_InverseViewMatrix[3][0] = position.x;
	m_InverseViewMatrix[3][1] = position.y;
	m_InverseViewMatrix[3][2] = position.z;
}

void CameraSystem::EditorCameraInput(GLFWwindow* window, float dt)
{
	glm::vec3 rotate{ 0.0f };
	if (glfwGetKey(window, keys.lookRight) == GLFW_PRESS) rotate.y += 1.0f;
	if (glfwGetKey(window, keys.lookLeft) == GLFW_PRESS) rotate.y -= 1.0f;
	if (glfwGetKey(window, keys.lookUp) == GLFW_PRESS) rotate.x += 1.0f;
	if (glfwGetKey(window, keys.lookDown) == GLFW_PRESS) rotate.x -= 1.0f;

	if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon())
	{
		m_Rotation += m_LookSpeed * dt * glm::normalize(rotate);
	}

	m_Rotation.x = glm::clamp(m_Rotation.x, -1.5f, 1.5f);
	m_Rotation.y = glm::mod(m_Rotation.y, glm::two_pi<float>());


	float yaw = m_Rotation.y;
	const glm::vec3 forwardDir{ sin(yaw), 0.0f, cos(yaw) };
	const glm::vec3 rightDir{ forwardDir.z, 0.0f, -forwardDir.x };
	glm::vec3 upDir{ 0.0f, -1.0f, 0.0f };

	glm::vec3 moveDir{ 0.0f };
	if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir += forwardDir;
	if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir -= forwardDir;
	if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir += rightDir;
	if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir -= rightDir;
	if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS) moveDir += upDir;
	if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS) moveDir -= upDir;

	if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon())
	{
		m_Position += m_MoveSpeed * dt * glm::normalize(moveDir);
	}

	SetViewYXZ(m_Position, m_Rotation);
}

void CameraSystem::SetViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up)
{
	SetViewDirection(position, target - position, up);
}

void CameraSystem::SetViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up)
{
	const glm::vec3 w{ glm::normalize(direction) };
	const glm::vec3 u{ glm::normalize(glm::cross(w, up)) };
	const glm::vec3 v{ glm::cross(w, u) };

	m_ViewMatrix = glm::mat4{ 1.f };
	m_ViewMatrix[0][0] = u.x;
	m_ViewMatrix[1][0] = u.y;
	m_ViewMatrix[2][0] = u.z;
	m_ViewMatrix[0][1] = v.x;
	m_ViewMatrix[1][1] = v.y;
	m_ViewMatrix[2][1] = v.z;
	m_ViewMatrix[0][2] = w.x;
	m_ViewMatrix[1][2] = w.y;
	m_ViewMatrix[2][2] = w.z;
	m_ViewMatrix[3][0] = -glm::dot(u, position);
	m_ViewMatrix[3][1] = -glm::dot(v, position);
	m_ViewMatrix[3][2] = -glm::dot(w, position);

	m_InverseViewMatrix = glm::mat4{ 1.f };
	m_InverseViewMatrix[0][0] = u.x;
	m_InverseViewMatrix[0][1] = u.y;
	m_InverseViewMatrix[0][2] = u.z;
	m_InverseViewMatrix[1][0] = v.x;
	m_InverseViewMatrix[1][1] = v.y;
	m_InverseViewMatrix[1][2] = v.z;
	m_InverseViewMatrix[2][0] = w.x;
	m_InverseViewMatrix[2][1] = w.y;
	m_InverseViewMatrix[2][2] = w.z;
	m_InverseViewMatrix[3][0] = position.x;
	m_InverseViewMatrix[3][1] = position.y;
	m_InverseViewMatrix[3][2] = position.z;
}
