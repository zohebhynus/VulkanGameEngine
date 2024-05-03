#include "PointLightRenderSystem.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/constants.hpp>
#include<stdexcept>
#include <cassert>
#include <map>

extern Coordinator m_Coord;


struct LightObjectPushConstant
{
	glm::vec4 position{};
	glm::vec4 color{};
	float radius;
};

PointLightRenderSystem::PointLightRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout) :m_Device(device)
{
	createPipelineLayout(globalSetLayout);
	createPipeline(renderPass);
}

PointLightRenderSystem::~PointLightRenderSystem()
{
	vkDestroyPipelineLayout(m_Device.device(), m_PipelineLayout, nullptr);
}

void PointLightRenderSystem::Update(FrameInfo& frameInfo, GlobalUBO& ubo)
{
	auto rotateLight = glm::rotate(glm::mat4(1.0f), frameInfo.frameTime, { 0.0f, -1.0f, 0.0f });
	int pointLightIndex = 0;
	int spotLightIndex = 0;

	//ubo.directionalLightData.direction = glm::vec4(point, 0.02f);

	auto rotateDirLight = glm::rotate(glm::mat4(1.0f), frameInfo.frameTime, { 0.0f, 0.0f, -1.0f });
	//auto re = rotateDirLight * glm::vec4(ubo.dirLightDirection.x, ubo.dirLightDirection.y, ubo.dirLightDirection.z, 1.0f);
	//ubo.dirLightDirection = glm::vec4(re.x, re.y, re.z, ubo.dirLightDirection.w);

	for (auto& entity : m_Entities)
	{
		assert(pointLightIndex <= MAX_POINT_LIGHTS && "Point lights exceed maximum specified");
		assert(spotLightIndex <= MAX_SPOT_LIGHTS && "Point lights exceed maximum specified");

		auto& transform = m_Coord.GetComponent<ECSTransformComponent>(entity);
		auto& lightObj = m_Coord.GetComponent<LightObjectComponent>(entity);

		// copy light data to ubo
		if (lightObj.isPoint)
		{
			ubo.pointLights[pointLightIndex].position = glm::vec4(transform.position, 1.0f);
			ubo.pointLights[pointLightIndex].color = glm::vec4(lightObj.lightColor, lightObj.lightIntensity);
			pointLightIndex++;
		}
		else
		{
			ubo.spotLights[spotLightIndex].position = glm::vec4(transform.position, 1.0f);
			ubo.spotLights[spotLightIndex].color = glm::vec4(lightObj.lightColor, lightObj.lightIntensity);
			ubo.spotLights[spotLightIndex].direction = glm::vec4(lightObj.lightDirection, 1.0f);
			ubo.spotLights[spotLightIndex].cutOffs = glm::vec4(lightObj.cutOff, lightObj.outerCutOff, 0.0f, 0.0f);
			spotLightIndex++;
		}
	}
	ubo.numOfActivePointLights = pointLightIndex;
	ubo.numOfActiveSpotLights = spotLightIndex;

}

void PointLightRenderSystem::Render(FrameInfo& frameInfo, GlobalUBO& globalUBO)
{

	//sort lights
	std::map<float, Entity> sorted;
	for (auto& entity : m_Entities)
	{
		auto& transform = m_Coord.GetComponent<ECSTransformComponent>(entity);

		//calculate distance
		auto offset = frameInfo.cameraSystem.GetEditorCameraPosition() - transform.position;
		float disSquared = glm::dot(offset, offset);
		sorted[disSquared] = entity;
	}

	m_Pipeline->bind(frameInfo.commandBuffer);
	vkCmdBindDescriptorSets(
		frameInfo.commandBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		m_PipelineLayout,
		0, 1,
		&frameInfo.globalDescriptorSet,
		0,
		nullptr);

	//iterate through sorted map in reverse order (Point and Spot Light Objects)
	for (auto it = sorted.rbegin(); it != sorted.rend(); it++)
	{
		auto& entity = it->second;

		auto& lightObj = m_Coord.GetComponent<LightObjectComponent>(entity);
		auto& transform = m_Coord.GetComponent<ECSTransformComponent>(entity);

		LightObjectPushConstant push{};
		push.position = glm::vec4(transform.position, lightObj.isPoint ? -1.0f : lightObj.cutOff);
		push.color = glm::vec4(lightObj.lightColor, lightObj.lightIntensity);
		push.radius = lightObj.lightObjectRadius;

		vkCmdPushConstants(
			frameInfo.commandBuffer,
			m_PipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0,
			sizeof(LightObjectPushConstant),
			&push);
		vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
	}
	
	// Directional Light Data
	LightObjectPushConstant push{};
	push.position = glm::vec4(
		-globalUBO.directionalLightData.direction.x, 
		-globalUBO.directionalLightData.direction.y, 
		-globalUBO.directionalLightData.direction.z, 
		globalUBO.directionalLightData.direction.w);
	push.color = globalUBO.directionalLightData.color;
	push.radius = 5.0f * globalUBO.directionalLightData.color.w;

	vkCmdPushConstants(
		frameInfo.commandBuffer,
		m_PipelineLayout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0,
		sizeof(LightObjectPushConstant),
		&push);
	vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
}

void PointLightRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
{
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT };
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(LightObjectPushConstant);

	std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(m_Device.device(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create pipeline layout");
	}
}

void PointLightRenderSystem::createPipeline(VkRenderPass renderpass)
{
	assert(m_PipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

	PipelineConfigInfo pipelineConfig{};
	Pipeline::DefaultPipelineConfigInfo(pipelineConfig);
	Pipeline::EnableAlphaBlending(pipelineConfig);

	pipelineConfig.bindingDescription.clear();
	pipelineConfig.attributeDescription.clear();
	pipelineConfig.renderPass = renderpass;
	pipelineConfig.pipelineLayout = m_PipelineLayout;
	pipelineConfig.multisampleInfo.rasterizationSamples = m_Device.msaaSampleCountFlagBits();

	m_Pipeline = std::make_unique<Pipeline>(m_Device, "Assets/Shaders/PointLight.vert.spv", "Assets/Shaders/PointLight.frag.spv", pipelineConfig);
}
