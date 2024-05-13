#pragma once

#include "CameraSystem.h"
#include "Descriptor.h"

#include "vulkan/vulkan.h"

#define MAX_POINT_LIGHTS 10
#define MAX_SPOT_LIGHTS 10

struct PointLight
{
	glm::vec4 position{}; // position x,y,z
	glm::vec4 color{};    // color r=x, g=y, b=z, a=intensity
};

struct SpotLight
{
	glm::vec4 position{};     // position x,y,z
	glm::vec4 color{};        // color r=x, g=y, b=z, a=intensity
	glm::vec4 direction{};    // direction x, y, z
	glm::vec4 cutOffs{};      // CutOffs x=innerCutoff y=outerCutoff

};

struct DirectionalLight
{
	glm::vec4 direction{ -5.0f, 30.0f, -2.0f, 0.06f }; // direction x, y, z, w=ambientStrength
	glm::vec4 color{ 1.0f, 1.0f, 1.0f , 0.7f };        // color r=x, g=y, b=z, a=intensity
};

struct EditorCameraData
{
	glm::mat4 projectionMatrix{ 1.0f };
	glm::mat4 viewMatrix{ 1.0f };
	glm::mat4 inverseViewMatrix{ 1.0f };
};

struct GlobalUBO
{
	EditorCameraData cameraData{};

	DirectionalLight directionalLightData{};

	PointLight pointLights[MAX_POINT_LIGHTS];
	SpotLight spotLights[MAX_SPOT_LIGHTS];

	alignas(4)int numOfActivePointLights;
	alignas(4)int numOfActiveSpotLights;
};

struct FrameInfo
{
	int FrameIndex;
	float frameTime;
	VkCommandBuffer commandBuffer;
	CameraSystem& cameraSystem;
	VkDescriptorSet globalDescriptorSet;
};