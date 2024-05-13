#include "SimpleRenderSystem.h"
#include "../../Instrumentation.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/constants.hpp>

#include <memory>
#include<stdexcept>
#include <cassert>

extern Coordinator m_Coord;




struct MainPushConstantData
{
	glm::mat4 modelMatrix{ 1.0f };
	glm::mat4 normalMatrix{ 1.0f };
};

struct PointShadowPassPushConstantData
{
	glm::mat4 modelMatrix{ 1.0f };
	int lightCount{};
	int faceCount{};
};

struct SpotShadowPassPushConstantData
{
	glm::mat4 modelMatrix{ 1.0f };
	int lightCount{};
};

struct CascadedShadowPassPushConstantData
{
	glm::mat4 modelMatrix{ 1.0f };
	int cascadeIndex{};
};


SimpleRenderSystem::SimpleRenderSystem(Device& device, VkRenderPass renderPass, std::vector<VkDescriptorSetLayout> setLayouts, DescriptorPool& descriptorPool) :m_Device(device)
{
	PrepareShadowPassUBO();

	PreparePointShadowCubeMaps();
	PreparePointShadowPassRenderPass();
	PreparePointShadowPassFramebuffers();

	PrepareSpotShadowMaps();
	PrepareSpotShadowPassRenderPass();
	PrepareSpotShadowPassFramebuffers();

	//PrepareShadowPassFramebuffer();
	PrepareCascadeShadowPass();

	createPipelineLayout(setLayouts, descriptorPool);
	createPipeline(renderPass);
}

SimpleRenderSystem::~SimpleRenderSystem()
{
	vkDestroyPipelineLayout(m_Device.device(), m_MainPipelineLayout, nullptr);
	vkDestroyPipelineLayout(m_Device.device(), m_ShadowPassPipelineLayout, nullptr);
	vkDestroyPipelineLayout(m_Device.device(), m_PointShadowPassPipelineLayout, nullptr);

	// Depth attachment
	vkDestroyImageView(m_Device.device(), m_ShadowPass.shadowMapImage.view, nullptr);
	vkDestroyImage(m_Device.device(), m_ShadowPass.shadowMapImage.image, nullptr);
	vkFreeMemory(m_Device.device(), m_ShadowPass.shadowMapImage.mem, nullptr);

	vkDestroyImageView(m_Device.device(), m_PointShadowPass.pointShadowMapImage.view, nullptr);
	vkDestroyImage(m_Device.device(), m_PointShadowPass.pointShadowMapImage.image, nullptr);
	vkFreeMemory(m_Device.device(), m_PointShadowPass.pointShadowMapImage.mem, nullptr);

	vkDestroyImageView(m_Device.device(), m_SpotShadowPass.spotShadowMapImage.view, nullptr);
	vkDestroyImage(m_Device.device(), m_SpotShadowPass.spotShadowMapImage.image, nullptr);
	vkFreeMemory(m_Device.device(), m_SpotShadowPass.spotShadowMapImage.mem, nullptr);
}

void SimpleRenderSystem::RenderShadowPass(FrameInfo frameInfo, GlobalUBO& globalUBO)
{
	PROFILE_FUNCTION();
	UpdateShadowPassBuffer(globalUBO);

	VkClearValue clearValues[2];
	clearValues[0].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = m_ShadowPass.renderPass;
	renderPassBeginInfo.framebuffer = m_ShadowPass.frameBuffer;
	renderPassBeginInfo.renderArea.extent.width = m_ShadowPass.width;
	renderPassBeginInfo.renderArea.extent.height = m_ShadowPass.height;
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = clearValues;

	vkCmdBeginRenderPass(frameInfo.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport {};
	viewport.width = (float)m_ShadowPass.width;
	viewport.height = (float)m_ShadowPass.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(frameInfo.commandBuffer, 0, 1, &viewport);

	VkRect2D scissor {};
	scissor.extent.width = m_ShadowPass.width;
	scissor.extent.height = m_ShadowPass.height; 
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	vkCmdSetScissor(frameInfo.commandBuffer, 0, 1, & scissor);


	// Depth bias (and slope) are used to avoid shadowing artifacts
// Constant depth bias factor (always applied)
	float depthBiasConstant = 1.25f;
		// Slope depth bias factor, applied depending on polygon's slope
	float depthBiasSlope = 1.75f;

	// Set depth bias (aka "Polygon offset")
	// Required to avoid shadow mapping artifacts
	vkCmdSetDepthBias(
		frameInfo.commandBuffer,
		depthBiasConstant,
		0.0f,
		depthBiasSlope);

	m_ShadowPassPipeline->bind(frameInfo.commandBuffer);

	std::vector<VkDescriptorSet> globSet = { frameInfo.globalDescriptorSet, m_ShadowPassDescriptorSet };
	vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ShadowPassPipelineLayout, 0, globSet.size(), globSet.data(), 0, nullptr);

	RenderGameObjects(frameInfo.commandBuffer, m_ShadowPassPipelineLayout, PushConstantType::MAIN, globSet.size(), false);

	vkCmdEndRenderPass(frameInfo.commandBuffer);
}

void SimpleRenderSystem::RenderCascadedShadowPass(FrameInfo frameInfo, GlobalUBO& globalUBO)
{
	UpdateCascades(globalUBO);

	m_CascadedShadowPassBuffer->writeToBuffer(&m_CascadedShadowPass.ubo);
	m_CascadedShadowPassBuffer->flush();

	VkClearValue clearValues[1];
	clearValues[0].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = m_CascadedShadowPass.renderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = m_CascadedShadowMapSize;
	renderPassBeginInfo.renderArea.extent.height = m_CascadedShadowMapSize;
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = clearValues;

	VkViewport viewport{};
	viewport.width = (float)m_CascadedShadowMapSize;
	viewport.height = (float)m_CascadedShadowMapSize;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(frameInfo.commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.extent.width = m_CascadedShadowMapSize;
	scissor.extent.height = m_CascadedShadowMapSize;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	vkCmdSetScissor(frameInfo.commandBuffer, 0, 1, &scissor);

	std::vector<VkDescriptorSet> globSet = { frameInfo.globalDescriptorSet, m_CascadedShadowPassDescriptorSet };
	vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_CascadedShadowPassPipelineLayout, 0, globSet.size(), globSet.data(), 0, nullptr);

	// One pass per cascade
	// The layer that this pass renders to is defined by the cascade's image view (selected via the cascade's descriptor set)
	for (uint32_t j = 0; j < CASCADE_SHADOW_MAP_COUNT; j++) 
	{
		renderPassBeginInfo.framebuffer = m_CascadedShadowPass.cascades[j].frameBuffer;
		vkCmdBeginRenderPass(frameInfo.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		m_CascadeIndex = j;

		m_CascadedShadowPassPipeline->bind(frameInfo.commandBuffer);

		RenderGameObjects(frameInfo.commandBuffer, m_CascadedShadowPassPipelineLayout, PushConstantType::CASCADEDSHADOW, globSet.size(), false);
		vkCmdEndRenderPass(frameInfo.commandBuffer);
	}
}

void SimpleRenderSystem::RenderPointShadowPass(FrameInfo frameInfo, GlobalUBO& globalUBO)
{
	PROFILE_FUNCTION();
	VkViewport viewport{};
	viewport.width = (float)m_PointShadowPass.width;
	viewport.height = (float)m_PointShadowPass.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(frameInfo.commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.extent.width = m_PointShadowPass.width;
	scissor.extent.height = m_PointShadowPass.height;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	vkCmdSetScissor(frameInfo.commandBuffer, 0, 1, &scissor);


	for (uint32_t i = 0; i < globalUBO.numOfActivePointLights; i++)
	{
		m_PointLightCount = i;
		for (uint32_t face = 0; face < 6; face++) {
			m_FaceCount = face;
			updateCubeFace(face, frameInfo, globalUBO);
		}
	}
}


void SimpleRenderSystem::RenderMainPass(FrameInfo frameInfo)
{
	PROFILE_FUNCTION();
	m_SpotShadowLightProjectionsBuffer->writeToBuffer(&m_SpotShadowLightProjectionsUBO);
	m_SpotShadowLightProjectionsBuffer->flush();

	m_MainPipeline->bind(frameInfo.commandBuffer);

	std::vector<VkDescriptorSet> globSet = 
	{ 
		frameInfo.globalDescriptorSet, 
		//m_ShadowPassDescriptorSet, m_ShadowMapDescriptorSet,
		m_CascadedShadowPassDescriptorSet, m_CascadedShadowMapDescriptorSet,
		m_PointShadowMapDescriptorSet,
		m_SpotShadowMapDescriptorSet
	};
	vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_MainPipelineLayout, 0, globSet.size(), globSet.data(), 0, nullptr);

	RenderGameObjects(frameInfo.commandBuffer, m_MainPipelineLayout, PushConstantType::MAIN, globSet.size(), true);
}

void SimpleRenderSystem::RenderGameObjects(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, PushConstantType type, int setCount, bool renderMaterial)
{
	//auto rotateCube = glm::rotate(glm::mat4(1.0f), frameInfo.frameTime, { -1.0f, -1.0f, -1.0f });
	for (auto& entity : m_Entities)
	{
		auto& transform = m_Coord.GetComponent<ECSTransformComponent>(entity);
		auto& model = m_Coord.GetComponent<ModelComponent>(entity);

		//transform.position = glm::vec3(rotateCube * glm::vec4(transform.position, 1.0f));

		if (type == SimpleRenderSystem::MAIN)
		{
			MainPushConstantData data{};
			data.modelMatrix = modelMatrix(transform.position, transform.rotation, transform.scale);
			data.normalMatrix = normalMatrix(transform.rotation, transform.scale);

			vkCmdPushConstants(
				commandBuffer,
				pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				sizeof(data),
				&data);
		}
		else if(type == SimpleRenderSystem::POINTSHADOW)
		{
			PointShadowPassPushConstantData data{};
			data.modelMatrix = modelMatrix(transform.position, transform.rotation, transform.scale);
			data.lightCount = m_PointLightCount;
			data.faceCount = m_FaceCount;

			vkCmdPushConstants(
				commandBuffer,
				pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				sizeof(data),
				&data);
		}
		else if (type == SimpleRenderSystem::SPOTSHADOW)
		{
			SpotShadowPassPushConstantData data{};
			data.modelMatrix = modelMatrix(transform.position, transform.rotation, transform.scale);
			data.lightCount = m_SpotLightIndex;

			vkCmdPushConstants(
				commandBuffer,
				pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				sizeof(data),
				&data);
		}
		else if (type == SimpleRenderSystem::CASCADEDSHADOW)
		{
			CascadedShadowPassPushConstantData data{};
			data.modelMatrix = modelMatrix(transform.position, transform.rotation, transform.scale);
			data.cascadeIndex = m_CascadeIndex;

			vkCmdPushConstants(
				commandBuffer,
				pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				sizeof(data),
				&data);
		}
		model.model->Bind(commandBuffer);
		model.model->Draw(commandBuffer, pipelineLayout, setCount, renderMaterial);
	}
}

void SimpleRenderSystem::createPipelineLayout(std::vector<VkDescriptorSetLayout> setLayouts, DescriptorPool& descriptorPool)
{
	std::vector<VkDescriptorSetLayout> mainSetLayouts;
	std::vector<VkDescriptorSetLayout> shadowPassSetLayouts;
	std::vector<VkDescriptorSetLayout> pointShadowPassSetLayouts;
	std::vector<VkDescriptorSetLayout> spotShadowPassSetLayouts;
	std::vector<VkDescriptorSetLayout> cascadedShadowPassSetLayouts;

	mainSetLayouts.push_back(setLayouts[0]);
	

	shadowPassSetLayouts.push_back(setLayouts[0]);
	

	pointShadowPassSetLayouts.push_back(setLayouts[0]);
	

	spotShadowPassSetLayouts.push_back(setLayouts[0]);

	cascadedShadowPassSetLayouts.push_back(setLayouts[0]);
	

	VkPushConstantRange mainPushConstantRange{};
	mainPushConstantRange.stageFlags = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT };
	mainPushConstantRange.offset = 0;
	mainPushConstantRange.size = sizeof(MainPushConstantData);

	VkPushConstantRange pointShadowPushConstantRange{};
	pointShadowPushConstantRange.stageFlags = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT };
	pointShadowPushConstantRange.offset = 0;
	pointShadowPushConstantRange.size = sizeof(PointShadowPassPushConstantData);

	VkPushConstantRange spotShadowPushConstantRange{};
	spotShadowPushConstantRange.stageFlags = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT };
	spotShadowPushConstantRange.offset = 0;
	spotShadowPushConstantRange.size = sizeof(SpotShadowPassPushConstantData);

	VkPushConstantRange cascadedShadowPushConstantRange{};
	cascadedShadowPushConstantRange.stageFlags = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT };
	cascadedShadowPushConstantRange.offset = 0;
	cascadedShadowPushConstantRange.size = sizeof(CascadedShadowPassPushConstantData);


	// ShadowPass Pipeline Layout
	//auto shadowPassUBOLayout = DescriptorSetLayout::Builder(m_Device)
	//	.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
	//	.build();
	//auto bufferInfo = m_ShadowPassBuffer->descriptorInfo();
	//DescriptorWriter(*shadowPassUBOLayout, descriptorPool)
	//	.writeBuffer(0, &bufferInfo)
	//	.build(m_ShadowPassDescriptorSet);
	//shadowPassSetLayouts.push_back(shadowPassUBOLayout->getDescriptorSetLayout());
	////shadowPassSetLayouts.push_back(setLayouts[1]);

	//VkPipelineLayoutCreateInfo shadowPassPipelineLayoutInfo{};
	//shadowPassPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	//shadowPassPipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(shadowPassSetLayouts.size());
	//shadowPassPipelineLayoutInfo.pSetLayouts = shadowPassSetLayouts.data();
	//shadowPassPipelineLayoutInfo.pushConstantRangeCount = 1;
	//shadowPassPipelineLayoutInfo.pPushConstantRanges = &mainPushConstantRange;

	//if (vkCreatePipelineLayout(m_Device.device(), &shadowPassPipelineLayoutInfo, nullptr, &m_ShadowPassPipelineLayout) != VK_SUCCESS)
	//{
	//	throw std::runtime_error("Failed to create SimpleRenderSystem:ShadowPassPipelineLayout");
	//}

	// Cascaded Shadow Pass
	auto cascadedShadowPassUBOLayout = DescriptorSetLayout::Builder(m_Device)
		.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.build();
	auto bufferInfoCas = m_CascadedShadowPassBuffer->descriptorInfo();
	DescriptorWriter(*cascadedShadowPassUBOLayout, descriptorPool)
		.writeBuffer(0, &bufferInfoCas)
		.build(m_CascadedShadowPassDescriptorSet);
	cascadedShadowPassSetLayouts.push_back(cascadedShadowPassUBOLayout->getDescriptorSetLayout());

	VkPipelineLayoutCreateInfo cascadedShadowPassPipelineLayoutInfo{};
	cascadedShadowPassPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	cascadedShadowPassPipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(cascadedShadowPassSetLayouts.size());
	cascadedShadowPassPipelineLayoutInfo.pSetLayouts = cascadedShadowPassSetLayouts.data();
	cascadedShadowPassPipelineLayoutInfo.pushConstantRangeCount = 1;
	cascadedShadowPassPipelineLayoutInfo.pPushConstantRanges = &mainPushConstantRange;

	if (vkCreatePipelineLayout(m_Device.device(), &cascadedShadowPassPipelineLayoutInfo, nullptr, &m_CascadedShadowPassPipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create SimpleRenderSystem:CascadedShadowPassPipelineLayout");
	}

	// Main Pipeline Layout
		// Shadow Map descriptorSet
	//auto shadowMapDescriptorSetLayout = DescriptorSetLayout::Builder(m_Device)
	//	.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
	//	.build();
	//VkDescriptorImageInfo shadowMapDescriptor{};
	//shadowMapDescriptor.sampler = m_ShadowPass.shadowMapSampler;
	//shadowMapDescriptor.imageView = m_ShadowPass.shadowMapImage.view;
	//shadowMapDescriptor.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	//DescriptorWriter(*shadowMapDescriptorSetLayout, descriptorPool)
	//	.writeImage(0, &shadowMapDescriptor)
	//	.build(m_ShadowMapDescriptorSet);
	//mainSetLayouts.push_back(shadowPassUBOLayout->getDescriptorSetLayout());
	//mainSetLayouts.push_back(shadowMapDescriptorSetLayout->getDescriptorSetLayout());

		// Cascaded Shadow Map descriptorSet
	auto cascadedShadowMapDescriptorSetLayout = DescriptorSetLayout::Builder(m_Device)
		.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build();
	VkDescriptorImageInfo cascadedshadowMapDescriptor{};
	cascadedshadowMapDescriptor.sampler = m_CascadedDepthMapObject.sampler;
	cascadedshadowMapDescriptor.imageView = m_CascadedDepthMapObject.view;
	cascadedshadowMapDescriptor.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	DescriptorWriter(*cascadedShadowMapDescriptorSetLayout, descriptorPool)
		.writeImage(0, &cascadedshadowMapDescriptor)
		.build(m_CascadedShadowMapDescriptorSet);
	mainSetLayouts.push_back(cascadedShadowPassUBOLayout->getDescriptorSetLayout());
	mainSetLayouts.push_back(cascadedShadowMapDescriptorSetLayout->getDescriptorSetLayout());

		// Point Shadow Map descriptorSet
	auto pointShadowMapDescriptorSetLayout = DescriptorSetLayout::Builder(m_Device)
		.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build();
	VkDescriptorImageInfo pointShadowMapDescriptor{};
	pointShadowMapDescriptor.sampler = m_PointShadowCubeMaps.cubeMapSampler;
	pointShadowMapDescriptor.imageView = m_PointShadowCubeMaps.cubeMapImage.view;
	pointShadowMapDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	DescriptorWriter(*pointShadowMapDescriptorSetLayout, descriptorPool)
		.writeImage(0, &pointShadowMapDescriptor)
		.build(m_PointShadowMapDescriptorSet);
	mainSetLayouts.push_back(pointShadowMapDescriptorSetLayout->getDescriptorSetLayout());

	// Spot Shadow Map descriptorSet
	auto spotShadowMapDescriptorSetLayout = DescriptorSetLayout::Builder(m_Device)
		.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.addBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
		.build();
	VkDescriptorImageInfo spotShadowMapDescriptor{};
	spotShadowMapDescriptor.sampler = m_SpotShadowMaps.cubeMapSampler;
	spotShadowMapDescriptor.imageView = m_SpotShadowMaps.cubeMapImage.view;
	spotShadowMapDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	auto bufferInfoTwo = m_SpotShadowLightProjectionsBuffer->descriptorInfo();
	DescriptorWriter(*spotShadowMapDescriptorSetLayout, descriptorPool)
		.writeImage(0, &spotShadowMapDescriptor)
		.writeBuffer(1, &bufferInfoTwo)
		.build(m_SpotShadowMapDescriptorSet);
	mainSetLayouts.push_back(spotShadowMapDescriptorSetLayout->getDescriptorSetLayout());
	mainSetLayouts.push_back(setLayouts[1]);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(mainSetLayouts.size());
	pipelineLayoutInfo.pSetLayouts = mainSetLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &mainPushConstantRange;

	if (vkCreatePipelineLayout(m_Device.device(), &pipelineLayoutInfo, nullptr, &m_MainPipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create SimpleRenderSystem:MainPipelineLayout");
	}


	// Point Shadow Pass Pipeline Layout
	auto pointShadowPassUBOLayout = DescriptorSetLayout::Builder(m_Device)
		.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.build();
	auto pointBufferInfo = m_PointShadowPassBuffer->descriptorInfo();
	DescriptorWriter(*pointShadowPassUBOLayout, descriptorPool)
		.writeBuffer(0, &pointBufferInfo)
		.build(m_PointShadowPassDescriptorSet);
	pointShadowPassSetLayouts.push_back(pointShadowPassUBOLayout->getDescriptorSetLayout());
	//pointShadowPassSetLayouts.push_back(setLayouts[1]);

	VkPipelineLayoutCreateInfo pointShadowPassPipelineLayoutInfo{};
	pointShadowPassPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pointShadowPassPipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(pointShadowPassSetLayouts.size());
	pointShadowPassPipelineLayoutInfo.pSetLayouts = pointShadowPassSetLayouts.data();
	pointShadowPassPipelineLayoutInfo.pushConstantRangeCount = 1;
	pointShadowPassPipelineLayoutInfo.pPushConstantRanges = &pointShadowPushConstantRange;

	if (vkCreatePipelineLayout(m_Device.device(), &pointShadowPassPipelineLayoutInfo, nullptr, &m_PointShadowPassPipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create SimpleRenderSystem:PointShadowPassPipelineLayout");
	}

	// Spot Shadow Pass Pipeline Layout
	auto spotShadowPassUBOLayout = DescriptorSetLayout::Builder(m_Device)
		.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.build();
	auto spotBufferInfo = m_SpotShadowPassBuffer->descriptorInfoForIndex(0); //check
	//auto spotBufferInfo = m_SpotShadowPassBuffer->descriptorInfo(sizeof(ShadowPassUBO)); //check
	DescriptorWriter(*spotShadowPassUBOLayout, descriptorPool)
		.writeBuffer(0, &spotBufferInfo)
		.build(m_SpotShadowPassDescriptorSet);
	spotShadowPassSetLayouts.push_back(spotShadowPassUBOLayout->getDescriptorSetLayout());
	//spotShadowPassSetLayouts.push_back(setLayouts[1]);

	VkPipelineLayoutCreateInfo spotShadowPassPipelineLayoutInfo{};
	spotShadowPassPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	spotShadowPassPipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(spotShadowPassSetLayouts.size());
	spotShadowPassPipelineLayoutInfo.pSetLayouts = spotShadowPassSetLayouts.data();
	spotShadowPassPipelineLayoutInfo.pushConstantRangeCount = 1;
	spotShadowPassPipelineLayoutInfo.pPushConstantRanges = &spotShadowPushConstantRange;

	if (vkCreatePipelineLayout(m_Device.device(), &spotShadowPassPipelineLayoutInfo, nullptr, &m_SpotShadowPassPipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create SimpleRenderSystem:SpotShadowPassPipelineLayout");
	}
}

void SimpleRenderSystem::createPipeline(VkRenderPass renderpass)
{
	// Main Pipeline
	assert(m_MainPipelineLayout != nullptr && "Cannot create SimpleRenderSystem:MainPipeline before MainPipelineLayout");

	PipelineConfigInfo pipelineConfig{};
	Pipeline::DefaultPipelineConfigInfo(pipelineConfig);
	pipelineConfig.renderPass = renderpass;
	pipelineConfig.pipelineLayout = m_MainPipelineLayout;
	pipelineConfig.multisampleInfo.rasterizationSamples = m_Device.msaaSampleCountFlagBits();

	m_MainPipeline = std::make_unique<Pipeline>(m_Device, "Assets/Shaders/Basic.vert.spv", "Assets/Shaders/Basic.frag.spv", pipelineConfig);

	// Point Shadow Pass Pipeline
	assert(m_PointShadowPassPipelineLayout != nullptr && "Cannot create SimpleRenderSystem:PointShadowPassPipeline before PointShadowPassPipelineLayout");

	pipelineConfig.renderPass = m_PointShadowPass.renderPass;
	pipelineConfig.pipelineLayout = m_PointShadowPassPipelineLayout;
	pipelineConfig.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	m_PointShadowPassPipeline = std::make_unique<Pipeline>(m_Device, "Assets/Shaders/PointShadowPass.vert.spv", "Assets/Shaders/PointShadowPass.frag.spv", pipelineConfig);

	// Spot Shadow Pass Pipeline
	assert(m_SpotShadowPassPipelineLayout != nullptr && "Cannot create SimpleRenderSystem:SpotShadowPassPipeline before SpotShadowPassPipelineLayout");

	pipelineConfig.renderPass = m_SpotShadowPass.renderPass;
	pipelineConfig.pipelineLayout = m_SpotShadowPassPipelineLayout;
	pipelineConfig.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	m_SpotShadowPassPipeline = std::make_unique<Pipeline>(m_Device, "Assets/Shaders/SpotShadowPass.vert.spv", "Assets/Shaders/SpotShadowPass.frag.spv", pipelineConfig);

	// Shadow Pass Pipeline
	//assert(m_ShadowPassPipelineLayout != nullptr && "Cannot create SimpleRenderSystem:ShadowPassPipeline before ShadowPassPipelineLayout");

	//pipelineConfig.colorBlendInfo.attachmentCount = 0;
	//pipelineConfig.rasterizationInfo.depthBiasEnable = VK_TRUE;

	//pipelineConfig.dynamicStateEnables.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
	//pipelineConfig.dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	//pipelineConfig.dynamicStateCreateInfo.pDynamicStates = pipelineConfig.dynamicStateEnables.data();
	//pipelineConfig.dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(pipelineConfig.dynamicStateEnables.size());

	//pipelineConfig.renderPass = m_ShadowPass.renderPass;
	//pipelineConfig.pipelineLayout = m_ShadowPassPipelineLayout;
	//pipelineConfig.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	//pipelineConfig.rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;

	//m_ShadowPassPipeline = std::make_unique<Pipeline>(m_Device, "Assets/Shaders/ShadowPass.vert.spv", "Assets/Shaders/ShadowPass.frag.spv", pipelineConfig);

	// Cascaded Shadow Pass Pipeline
	assert(m_CascadedShadowPassPipelineLayout != nullptr && "Cannot create SimpleRenderSystem:CascadedShadowPassPipeline before CascadedShadowPassPipelineLayout");

	pipelineConfig.colorBlendInfo.attachmentCount = 0;

	pipelineConfig.renderPass = m_CascadedShadowPass.renderPass;
	pipelineConfig.pipelineLayout = m_CascadedShadowPassPipelineLayout;
	pipelineConfig.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	pipelineConfig.rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;

	m_CascadedShadowPassPipeline = std::make_unique<Pipeline>(m_Device, "Assets/Shaders/CascadedShadowPass.vert.spv", "Assets/Shaders/CascadedShadowPass.frag.spv", pipelineConfig);
}

void SimpleRenderSystem::PrepareShadowPassRenderpass()
{
	VkAttachmentDescription attachmentDescription{};
	attachmentDescription.format = m_ShadowPassImageFormat;
	attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;							// Clear depth at beginning of the render pass
	attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;						// We will read from depth, so it's important to store the depth attachment results
	attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;					// We don't care about initial layout of the attachment
	attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;// Attachment will be transitioned to shader read at render pass end

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 0;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;			// Attachment will be used as depth/stencil during render pass

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 0;													// No color attachments
	subpass.pDepthStencilAttachment = &depthReference;									// Reference to our depth attachment

	// Use subpass dependencies for layout transitions
	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassCreateInfo{};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &attachmentDescription;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassCreateInfo.pDependencies = dependencies.data();

	if (vkCreateRenderPass(m_Device.device(), &renderPassCreateInfo, nullptr, &m_ShadowPass.renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create offscreen render pass!");
	}
}

void SimpleRenderSystem::PrepareShadowPassFramebuffer()
{
	m_ShadowPass.width = m_ShadowMapSize;
	m_ShadowPass.height = m_ShadowMapSize;

	// For shadow mapping we only need a depth attachment
	VkImageCreateInfo image{};
	image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image.imageType = VK_IMAGE_TYPE_2D;
	image.extent.width = m_ShadowPass.width;
	image.extent.height = m_ShadowPass.height;
	image.extent.depth = 1;
	image.mipLevels = 1;
	image.arrayLayers = 1;
	image.samples = VK_SAMPLE_COUNT_1_BIT;
	image.tiling = VK_IMAGE_TILING_OPTIMAL;
	image.format = m_ShadowPassImageFormat;																// Depth stencil attachment
	image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;		// We will sample directly from the depth attachment for the shadow mapping

	if (vkCreateImage(m_Device.device(), &image, nullptr, &m_ShadowPass.shadowMapImage.image) != VK_SUCCESS) {
		throw std::runtime_error("failed to create offscreen depth Image!");
	}

	VkMemoryAllocateInfo memAlloc{};
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	VkMemoryRequirements memReqs;

	vkGetImageMemoryRequirements(m_Device.device(), m_ShadowPass.shadowMapImage.image, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = m_Device.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(m_Device.device(), &memAlloc, nullptr, &m_ShadowPass.shadowMapImage.mem) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate offscreen depth Image memory!");
	}

	if (vkBindImageMemory(m_Device.device(), m_ShadowPass.shadowMapImage.image, m_ShadowPass.shadowMapImage.mem, 0) != VK_SUCCESS) {
		throw std::runtime_error("failed to bind offscreen depth Image memory!");
	}

	VkImageViewCreateInfo depthStencilView{};
	depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthStencilView.format = m_ShadowPassImageFormat;
	depthStencilView.subresourceRange = {};
	depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthStencilView.subresourceRange.baseMipLevel = 0;
	depthStencilView.subresourceRange.levelCount = 1;
	depthStencilView.subresourceRange.baseArrayLayer = 0;
	depthStencilView.subresourceRange.layerCount = 1;
	depthStencilView.image = m_ShadowPass.shadowMapImage.image;

	if (vkCreateImageView(m_Device.device(), &depthStencilView, nullptr, &m_ShadowPass.shadowMapImage.view) != VK_SUCCESS) {
		throw std::runtime_error("failed to create offscreen Image view!");
	}

	// Create sampler to sample from to depth attachment
	// Used to sample in the fragment shader for shadowed rendering
	//VkFilter shadowmap_filter = vks::tools::formatIsFilterable(physicalDevice, offscreenDepthFormat, VK_IMAGE_TILING_OPTIMAL) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

	VkSamplerCreateInfo sampler{};
	sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 1.0f;
	sampler.minLod = 0.0f;
	sampler.maxLod = 1.0f;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	if (vkCreateSampler(m_Device.device(), &sampler, nullptr, &m_ShadowPass.shadowMapSampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create offscreen depth sampler!");
	}

	PrepareShadowPassRenderpass();

	// Create frame buffer
	VkFramebufferCreateInfo fbufCreateInfo{};
	fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbufCreateInfo.renderPass = m_ShadowPass.renderPass;
	fbufCreateInfo.attachmentCount = 1;
	fbufCreateInfo.pAttachments = &m_ShadowPass.shadowMapImage.view;
	fbufCreateInfo.width = m_ShadowPass.width;
	fbufCreateInfo.height = m_ShadowPass.height;
	fbufCreateInfo.layers = 1;

	if (vkCreateFramebuffer(m_Device.device(), &fbufCreateInfo, nullptr, &m_ShadowPass.frameBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create offscreen depth frame buffer!");
	}
}

void SimpleRenderSystem::PrepareShadowPassUBO()
{
	//Direction Light Shadow Pass
	//m_ShadowPassBuffer = std::make_unique<Buffer>(
	//	m_Device,
	//	sizeof(ShadowPassUBO),
	//	1,
	//	VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
	//	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	//m_ShadowPassBuffer->map();

	// Cascaded Shadow Map
	m_CascadedShadowPassBuffer = std::make_unique<Buffer>(
		m_Device,
		sizeof(CascadedShadowPassUBO),
		1,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	m_CascadedShadowPassBuffer->map();

	//Point Light Shadow Pass
	m_PointShadowPassBuffer = std::make_unique<Buffer>(
		m_Device,
		sizeof(PointShadowPassViewMatrixUBO),
		1,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	m_PointShadowPassBuffer->map();

	float aspect = (float)m_PointShadowMapSize / (float)m_PointShadowMapSize;
	glm::mat4 proj = glm::perspective(glm::radians(90.0f), aspect, 0.1f, 25.0f);
	for (int i = 0; i < 6; i++)
	{
		glm::mat4 view = glm::mat4(1.0f);
		glm::vec3 pos = glm::vec3(0.0f);
		switch (i)
		{
		case 0: // POSITIVE_X
			view = glm::lookAt(pos, pos + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));
			break;
		case 1:	// NEGATIVE_X
			view = glm::lookAt(pos, pos + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));
			break;
		case 2:	// POSITIVE_Y
			view = glm::lookAt(pos, pos + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0));
			break;
		case 3:	// NEGATIVE_Y
			view = glm::lookAt(pos, pos + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0));
			break;
		case 4:	// POSITIVE_Z
			view = glm::lookAt(pos, pos + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0));
			break;
		case 5:	// NEGATIVE_Z
			view = glm::lookAt(pos, pos + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0));
			break;
		}
		m_PointShadowPassUBO.faceViewMatrix[i] = proj * view;
	}
	m_PointShadowPassBuffer->writeToBuffer(&m_PointShadowPassUBO);
	m_PointShadowPassBuffer->flush();

	//Spot Shadow Pass
	m_SpotShadowPassBuffer = std::make_unique<Buffer>(
		m_Device,
		sizeof(ShadowPassUBO),
		MAX_SPOT_LIGHTS,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		m_Device.properties.limits.minMemoryMapAlignment);
	m_SpotShadowPassBuffer->map();

	m_SpotShadowLightProjectionsBuffer = std::make_unique<Buffer>(
		m_Device,
		sizeof(SpotShadowLightProjectionsUBO),
		1,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	m_SpotShadowLightProjectionsBuffer->map();
}

void SimpleRenderSystem::UpdateShadowPassBuffer(GlobalUBO& globalUBO)
{
	glm::mat4 orthgonalProjection = glm::ortho(-30.0f, 30.0f, -30.0f, 30.0f, 0.1f, 100.0f);
	glm::mat4 lightView = glm::lookAt(glm::vec3(-globalUBO.directionalLightData.direction), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	m_ShadowPassUBO.lightProjection = orthgonalProjection * lightView;

	m_ShadowPassBuffer->writeToBuffer(&m_ShadowPassUBO);
	m_ShadowPassBuffer->flush();
}

void SimpleRenderSystem::PrepareCascadeShadowPass()
{
	VkFormat depthFormat = m_Device.DepthFormat();

	// Render Pass
	VkAttachmentDescription attachmentDescription{};
	attachmentDescription.format = depthFormat;
	attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 0;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 0;
	subpass.pDepthStencilAttachment = &depthReference;

	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassCreateInfo{};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &attachmentDescription;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassCreateInfo.pDependencies = dependencies.data();

	vkCreateRenderPass(m_Device.device(), &renderPassCreateInfo, nullptr, &m_CascadedShadowPass.renderPass);

	// Main Depth Map Imag and View
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = m_CascadedShadowMapSize;
	imageInfo.extent.height = m_CascadedShadowMapSize;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = CASCADE_SHADOW_MAP_COUNT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.format = depthFormat;
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	vkCreateImage(m_Device.device(), &imageInfo, nullptr, &m_CascadedDepthMapObject.image);
	VkMemoryAllocateInfo memAlloc{};
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(m_Device.device(), m_CascadedDepthMapObject.image, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = m_Device.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vkAllocateMemory(m_Device.device(), &memAlloc, nullptr, &m_CascadedDepthMapObject.mem);
	vkBindImageMemory(m_Device.device(), m_CascadedDepthMapObject.image, m_CascadedDepthMapObject.mem, 0);

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	viewInfo.format = depthFormat;
	viewInfo.subresourceRange = {};
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = CASCADE_SHADOW_MAP_COUNT;
	viewInfo.image = m_CascadedDepthMapObject.image;
	vkCreateImageView(m_Device.device(), &viewInfo, nullptr, &m_CascadedDepthMapObject.view);

	// Framebuffer and image view per cascade
	for (uint32_t i = 0; i < CASCADE_SHADOW_MAP_COUNT; i++) {
		// Image view for this cascade's layer (inside the depth map)
		// This view is used to render to that specific depth image layer
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		viewInfo.format = depthFormat;
		viewInfo.subresourceRange = {};
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = i;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.image = m_CascadedDepthMapObject.image;
		vkCreateImageView(m_Device.device(), &viewInfo, nullptr, &m_CascadedShadowPass.cascades[i].view);
		// Framebuffer
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_CascadedShadowPass.renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &m_CascadedShadowPass.cascades[i].view;
		framebufferInfo.width = m_CascadedShadowMapSize;
		framebufferInfo.height = m_CascadedShadowMapSize;
		framebufferInfo.layers = 1;
		vkCreateFramebuffer(m_Device.device(), &framebufferInfo, nullptr, &m_CascadedShadowPass.cascades[i].frameBuffer);
	}

	// Sampler for caseded depth map reading
	// Shared sampler for cascade depth reads
	VkSamplerCreateInfo sampler{};
	sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 1.0f;
	sampler.minLod = 0.0f;
	sampler.maxLod = 1.0f;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	vkCreateSampler(m_Device.device(), &sampler, nullptr, &m_CascadedDepthMapObject.sampler);
}

void SimpleRenderSystem::UpdateCascades(GlobalUBO& ubo)
{
	float cascadeSplits[CASCADE_SHADOW_MAP_COUNT];

	float nearClip = 0.1f;
	float farClip = 100.0f;
	float clipRange = farClip - nearClip;

	float minZ = nearClip;
	float maxZ = nearClip + clipRange;

	float range = maxZ - minZ;
	float ratio = maxZ / minZ;

	float cascadeSplitLambda = 0.000001f;

	// Calculate split depths based on view camera frustum
	// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
	for (uint32_t i = 0; i < CASCADE_SHADOW_MAP_COUNT; i++) {
		float p = (i + 1) / static_cast<float>(CASCADE_SHADOW_MAP_COUNT);
		float log = minZ * std::pow(ratio, p);
		float uniform = minZ + range * p;
		float d = cascadeSplitLambda * (log - uniform) + uniform;
		cascadeSplits[i] = (d - nearClip) / clipRange;
	}

	// Calculate orthographic projection matrix for each cascade
	float lastSplitDist = 0.0;
	for (uint32_t i = 0; i < CASCADE_SHADOW_MAP_COUNT; i++) {
		float splitDist = cascadeSplits[i];

		glm::vec3 frustumCorners[8] = {
			glm::vec3(-1.0f,  1.0f, 0.0f),
			glm::vec3(1.0f,  1.0f, 0.0f),
			glm::vec3(1.0f, -1.0f, 0.0f),
			glm::vec3(-1.0f, -1.0f, 0.0f),
			glm::vec3(-1.0f,  1.0f,  1.0f),
			glm::vec3(1.0f,  1.0f,  1.0f),
			glm::vec3(1.0f, -1.0f,  1.0f),
			glm::vec3(-1.0f, -1.0f,  1.0f),
		};

		// Project frustum corners into world space
		glm::mat4 invCam = glm::inverse(ubo.cameraData.projectionMatrix * ubo.cameraData.viewMatrix);
		for (uint32_t j = 0; j < 8; j++) {
			glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[j], 1.0f);
			frustumCorners[j] = invCorner / invCorner.w;
		}

		for (uint32_t j = 0; j < 4; j++) {
			glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
			frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
			frustumCorners[j] = frustumCorners[j] + (dist * lastSplitDist);
		}

		// Get frustum center
		glm::vec3 frustumCenter = glm::vec3(0.0f);
		for (uint32_t j = 0; j < 8; j++) {
			frustumCenter += frustumCorners[j];
		}
		frustumCenter /= 8.0f;

		float radius = 0.0f;
		for (uint32_t j = 0; j < 8; j++) {
			float distance = glm::length(frustumCorners[j] - frustumCenter);
			radius = glm::max(radius, distance);
		}
		radius = std::ceil(radius * 16.0f) / 16.0f;

		glm::vec3 maxExtents = glm::vec3(radius);
		glm::vec3 minExtents = -maxExtents;

		glm::vec3 lightDir = glm::normalize(ubo.directionalLightData.direction);
		glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

		// Store split distance and matrix in cascade
		m_CascadedShadowPass.ubo.splitDepths[i] = (nearClip + splitDist * clipRange) * -1.0f;
		m_CascadedShadowPass.ubo.viewProjMats[i] = lightOrthoMatrix * lightViewMatrix;

		lastSplitDist = cascadeSplits[i];
	}
}

void SimpleRenderSystem::PreparePointShadowCubeMaps()
{
	m_PointShadowCubeMaps.width = m_PointShadowMapSize;
	m_PointShadowCubeMaps.height = m_PointShadowMapSize;

	// Cube map image description
	VkImageCreateInfo imageCreateInfo{};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = m_PointShadowPassImageFormat;
	imageCreateInfo.extent = { m_PointShadowCubeMaps.width, m_PointShadowCubeMaps.height, 1};
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 6 * MAX_POINT_LIGHTS;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	VkMemoryAllocateInfo memAllocInfo{};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	VkMemoryRequirements memReqs;

	VkCommandBuffer layoutCmd = m_Device.beginSingleTimeCommands();

	// Create cube map image
	if (vkCreateImage(m_Device.device(), &imageCreateInfo, nullptr, &m_PointShadowCubeMaps.cubeMapImage.image))
	{
		throw std::runtime_error("failed to create Point Shadow cube map image!");
	}

	vkGetImageMemoryRequirements(m_Device.device(), m_PointShadowCubeMaps.cubeMapImage.image, &memReqs);

	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = m_Device.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(m_Device.device(), &memAllocInfo, nullptr, &m_PointShadowCubeMaps.cubeMapImage.mem))
	{
		throw std::runtime_error("failed to create Point Shadow mem alloc for cube map image");
	}

	if (vkBindImageMemory(m_Device.device(), m_PointShadowCubeMaps.cubeMapImage.image, m_PointShadowCubeMaps.cubeMapImage.mem, 0))
	{
		throw std::runtime_error("failed to create Point Shadow bind cube map memory");
	}

	// Image barrier for optimal image (target)
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.layerCount = 6 * MAX_POINT_LIGHTS;
	m_Device.TransitionImageLayout(
		layoutCmd,
		m_PointShadowCubeMaps.cubeMapImage.image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		subresourceRange,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	m_Device.endSingleTimeCommands(layoutCmd);

	// Create sampler next
	VkSamplerCreateInfo sampler{};
	sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 1.0f;
	sampler.compareOp = VK_COMPARE_OP_NEVER;
	sampler.minLod = 0.0f;
	sampler.maxLod = 1.0f;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	//sampler.maxAnisotropy = m_Device.GetPhysicalDeviceProperties().limits.maxSamplerAnisotropy;
	//sampler.anisotropyEnable = VK_TRUE;


	if (vkCreateSampler(m_Device.device(), &sampler, nullptr, &m_PointShadowCubeMaps.cubeMapSampler))
	{
		throw std::runtime_error("failed to create Point Shadow cube map sampler");
	}

	// Create image view
	VkImageViewCreateInfo view{};
	view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view.image = VK_NULL_HANDLE;
	view.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	view.format = m_PointShadowPassImageFormat;
	view.components = { VK_COMPONENT_SWIZZLE_R };
	view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	view.subresourceRange.layerCount = 6 * MAX_POINT_LIGHTS;
	view.image = m_PointShadowCubeMaps.cubeMapImage.image;

	if (vkCreateImageView(m_Device.device(), &view, nullptr, &m_PointShadowCubeMaps.cubeMapImage.view))
	{
		throw std::runtime_error("failed to create Point Shadow cube map image view");
	}

	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.subresourceRange.layerCount = 1;
	view.image = m_PointShadowCubeMaps.cubeMapImage.image;

	for (int i = 0; i < MAX_POINT_LIGHTS; i++)
	{
		for (uint32_t j = 0; j < 6; j++)
		{
			view.subresourceRange.baseArrayLayer = j + (6 * i);
			if (vkCreateImageView(m_Device.device(), &view, nullptr, &m_PointShadowCubeMapImageViews[i][j]))
			{
				throw std::runtime_error("failed to create Point Shadow cube map image view");
			}
		}
	}
}

void SimpleRenderSystem::PreparePointShadowPassRenderPass()
{
	VkAttachmentDescription attachmentDescriptions[2] = {};

	attachmentDescriptions[0].format = m_PointShadowPassImageFormat;
	attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Depth attachment
	m_PointShadowPassDepthFormat = m_Device.DepthFormat();
	attachmentDescriptions[1].format = m_PointShadowPassDepthFormat;
	attachmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorReference;
	subpass.pDepthStencilAttachment = &depthReference;

	VkRenderPassCreateInfo renderPassCreateInfo{};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 2;
	renderPassCreateInfo.pAttachments = attachmentDescriptions;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;

	vkCreateRenderPass(m_Device.device(), &renderPassCreateInfo, nullptr, &m_PointShadowPass.renderPass);
}

void SimpleRenderSystem::PreparePointShadowPassFramebuffers()
{
	m_PointShadowPass.width = m_PointShadowMapSize;
	m_PointShadowPass.height = m_PointShadowMapSize;

	// Color attachment
	VkImageCreateInfo imageCreateInfo{};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = m_PointShadowPassImageFormat;
	imageCreateInfo.extent.width = m_PointShadowPass.width;
	imageCreateInfo.extent.height = m_PointShadowPass.height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	// Image of the framebuffer is blit source
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;



	VkCommandBuffer layoutCmd = m_Device.beginSingleTimeCommands();

	// Depth stencil attachment
	imageCreateInfo.format = m_PointShadowPassDepthFormat;
	imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VkImageViewCreateInfo depthStencilView{};
	depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthStencilView.format = m_PointShadowPassDepthFormat;
	depthStencilView.flags = 0;
	depthStencilView.subresourceRange = {};
	depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (m_PointShadowPassDepthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
		depthStencilView.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	depthStencilView.subresourceRange.baseMipLevel = 0;
	depthStencilView.subresourceRange.levelCount = 1;
	depthStencilView.subresourceRange.baseArrayLayer = 0;
	depthStencilView.subresourceRange.layerCount = 1;

	if (vkCreateImage(m_Device.device(), &imageCreateInfo, nullptr, &m_PointShadowPass.pointShadowMapImage.image))
	{
		throw std::runtime_error("failed to create Point Shadow depth stencil attachment!");
	}

	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(m_Device.device(), m_PointShadowPass.pointShadowMapImage.image, &memReqs);

	VkMemoryAllocateInfo memAlloc{};
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = m_Device.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(m_Device.device(), &memAlloc, nullptr, &m_PointShadowPass.pointShadowMapImage.mem))
	{
		throw std::runtime_error("failed to create Point Shadow mem alloc for Map Image");
	}

	if (vkBindImageMemory(m_Device.device(), m_PointShadowPass.pointShadowMapImage.image, m_PointShadowPass.pointShadowMapImage.mem, 0))
	{
		throw std::runtime_error("failed to create Point Shadow bind image memory");
	}

	VkImageAspectFlags temp = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	m_Device.TransitionImageLayout(
		layoutCmd,
		m_PointShadowPass.pointShadowMapImage.image,
		VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	m_Device.endSingleTimeCommands(layoutCmd);

	depthStencilView.image = m_PointShadowPass.pointShadowMapImage.image;
	if (vkCreateImageView(m_Device.device(), &depthStencilView, nullptr, &m_PointShadowPass.pointShadowMapImage.view))
	{
		throw std::runtime_error("failed to create Point Shadow depth stencil image viewy");
	}


	VkImageView attachments[2];
	attachments[1] = m_PointShadowPass.pointShadowMapImage.view;

	VkFramebufferCreateInfo fbufCreateInfo{};
	fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbufCreateInfo.renderPass = m_PointShadowPass.renderPass;
	fbufCreateInfo.attachmentCount = 2;
	fbufCreateInfo.pAttachments = attachments;
	fbufCreateInfo.width = m_PointShadowPass.width;
	fbufCreateInfo.height = m_PointShadowPass.height;
	fbufCreateInfo.layers = 1;
	for (int i = 0; i < MAX_POINT_LIGHTS; i++)
	{
		for (uint32_t j = 0; j < 6; j++)
		{
			attachments[0] = m_PointShadowCubeMapImageViews[i][j];
			if (vkCreateFramebuffer(m_Device.device(), &fbufCreateInfo, nullptr, &m_PointShadowPass.frameBuffers[i][j]))
			{
				throw std::runtime_error("failed to create Point Shadow depth stencil image viewy");
			}
		}
	}
}

void SimpleRenderSystem::updateCubeFace(uint32_t faceIndex, FrameInfo frameInfo, GlobalUBO& ubo)
{
	VkClearValue clearValues[2];
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = m_PointShadowPass.renderPass;
	renderPassBeginInfo.framebuffer = m_PointShadowPass.frameBuffers[m_PointLightCount][faceIndex];
	renderPassBeginInfo.renderArea.extent.width = m_PointShadowPass.width;
	renderPassBeginInfo.renderArea.extent.height = m_PointShadowPass.height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;


	// Render scene from cube face's point of view
	vkCmdBeginRenderPass(frameInfo.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	m_PointShadowPassPipeline->bind(frameInfo.commandBuffer);

	std::vector<VkDescriptorSet> globSet = { frameInfo.globalDescriptorSet, m_PointShadowPassDescriptorSet };
	vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PointShadowPassPipelineLayout, 0, globSet.size(), globSet.data(), 0, nullptr);

	RenderGameObjects(frameInfo.commandBuffer, m_PointShadowPassPipelineLayout, PushConstantType::POINTSHADOW, globSet.size(), false);

	vkCmdEndRenderPass(frameInfo.commandBuffer);
}

void SimpleRenderSystem::PrepareSpotShadowMaps()
{
	m_SpotShadowMaps.width = m_SpotShadowMapSize;
	m_SpotShadowMaps.height = m_SpotShadowMapSize;

	// Cube map image description
	VkImageCreateInfo imageCreateInfo{};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = m_SpotShadowPassImageFormat;
	imageCreateInfo.extent = { m_SpotShadowMaps.width, m_SpotShadowMaps.height, 1 };
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = MAX_SPOT_LIGHTS;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	VkMemoryAllocateInfo memAllocInfo{};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	VkMemoryRequirements memReqs;

	VkCommandBuffer layoutCmd = m_Device.beginSingleTimeCommands();

	// Create cube map image
	if (vkCreateImage(m_Device.device(), &imageCreateInfo, nullptr, &m_SpotShadowMaps.cubeMapImage.image))
	{
		throw std::runtime_error("failed to create Spot Shadow map image!");
	}

	vkGetImageMemoryRequirements(m_Device.device(), m_SpotShadowMaps.cubeMapImage.image, &memReqs);

	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = m_Device.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(m_Device.device(), &memAllocInfo, nullptr, &m_SpotShadowMaps.cubeMapImage.mem))
	{
		throw std::runtime_error("failed to create Point Shadow mem alloc for cube map image");
	}

	if (vkBindImageMemory(m_Device.device(), m_SpotShadowMaps.cubeMapImage.image, m_SpotShadowMaps.cubeMapImage.mem, 0))
	{
		throw std::runtime_error("failed to create Spot Shadow bind map memory");
	}

	// Image barrier for optimal image (target)
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.layerCount = MAX_SPOT_LIGHTS;
	m_Device.TransitionImageLayout(
		layoutCmd,
		m_SpotShadowMaps.cubeMapImage.image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		subresourceRange,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	m_Device.endSingleTimeCommands(layoutCmd);

	// Create sampler next
	VkSamplerCreateInfo sampler{};
	sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler.addressModeV = sampler.addressModeU;
	sampler.addressModeW = sampler.addressModeU;
	sampler.mipLodBias = 0.0f;
	sampler.maxAnisotropy = 1.0f;
	sampler.compareOp = VK_COMPARE_OP_NEVER;
	sampler.minLod = 0.0f;
	sampler.maxLod = 1.0f;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	sampler.maxAnisotropy = m_Device.GetPhysicalDeviceProperties().limits.maxSamplerAnisotropy;
	sampler.anisotropyEnable = VK_TRUE;


	if (vkCreateSampler(m_Device.device(), &sampler, nullptr, &m_SpotShadowMaps.cubeMapSampler))
	{
		throw std::runtime_error("failed to create Spot Shadow map sampler");
	}

	// Create image view
	VkImageViewCreateInfo view{};
	view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view.image = VK_NULL_HANDLE;
	view.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	view.format = m_SpotShadowPassImageFormat;
	view.components = { VK_COMPONENT_SWIZZLE_R };
	view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	view.subresourceRange.layerCount = MAX_SPOT_LIGHTS;
	view.image = m_SpotShadowMaps.cubeMapImage.image;

	if (vkCreateImageView(m_Device.device(), &view, nullptr, &m_SpotShadowMaps.cubeMapImage.view))
	{
		throw std::runtime_error("failed to create Spot Shadow map image view");
	}

	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.subresourceRange.layerCount = 1;
	view.image = m_SpotShadowMaps.cubeMapImage.image;

	for (uint32_t i = 0; i < MAX_SPOT_LIGHTS; i++)
	{
		view.subresourceRange.baseArrayLayer = i;
		if (vkCreateImageView(m_Device.device(), &view, nullptr, &m_SpotShadowPass.imageViews[i]))
		{
			throw std::runtime_error("failed to create Spot Shadow map image view");
		}
	}
}

void SimpleRenderSystem::PrepareSpotShadowPassRenderPass()
{
	VkAttachmentDescription attachmentDescriptions[2] = {};

	attachmentDescriptions[0].format = m_SpotShadowPassImageFormat;
	attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Depth attachment
	m_SpotShadowPassDepthFormat = m_Device.DepthFormat();
	attachmentDescriptions[1].format = m_SpotShadowPassDepthFormat;
	attachmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorReference;
	subpass.pDepthStencilAttachment = &depthReference;

	VkRenderPassCreateInfo renderPassCreateInfo{};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 2;
	renderPassCreateInfo.pAttachments = attachmentDescriptions;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;

	vkCreateRenderPass(m_Device.device(), &renderPassCreateInfo, nullptr, &m_SpotShadowPass.renderPass);
}

void SimpleRenderSystem::PrepareSpotShadowPassFramebuffers()
{

	m_SpotShadowPass.width = m_SpotShadowMapSize;
	m_SpotShadowPass.height = m_SpotShadowMapSize;

	// Color attachment
	VkImageCreateInfo imageCreateInfo{};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = m_SpotShadowPassImageFormat;
	imageCreateInfo.extent.width = m_SpotShadowPass.width;
	imageCreateInfo.extent.height = m_SpotShadowPass.height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	// Image of the framebuffer is blit source
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkImageViewCreateInfo colorImageView{};
	colorImageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	colorImageView.format = m_SpotShadowPassImageFormat;
	colorImageView.flags = 0;
	colorImageView.subresourceRange = {};
	colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorImageView.subresourceRange.baseMipLevel = 0;
	colorImageView.subresourceRange.levelCount = 1;
	colorImageView.subresourceRange.baseArrayLayer = 0;
	colorImageView.subresourceRange.layerCount = 1;

	VkCommandBuffer layoutCmd = m_Device.beginSingleTimeCommands();

	// Depth stencil attachment
	imageCreateInfo.format = m_SpotShadowPassDepthFormat;
	imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VkImageViewCreateInfo depthStencilView{};
	depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthStencilView.format = m_SpotShadowPassDepthFormat;
	depthStencilView.flags = 0;
	depthStencilView.subresourceRange = {};
	depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (m_SpotShadowPassDepthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
		depthStencilView.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	depthStencilView.subresourceRange.baseMipLevel = 0;
	depthStencilView.subresourceRange.levelCount = 1;
	depthStencilView.subresourceRange.baseArrayLayer = 0;
	depthStencilView.subresourceRange.layerCount = 1;

	if (vkCreateImage(m_Device.device(), &imageCreateInfo, nullptr, &m_SpotShadowPass.spotShadowMapImage.image))
	{
		throw std::runtime_error("failed to create Point Shadow depth stencil attachment!");
	}

	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(m_Device.device(), m_SpotShadowPass.spotShadowMapImage.image, &memReqs);

	VkMemoryAllocateInfo memAlloc{};
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = m_Device.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(m_Device.device(), &memAlloc, nullptr, &m_SpotShadowPass.spotShadowMapImage.mem))
	{
		throw std::runtime_error("failed to create Point Shadow mem alloc for Map Image");
	}

	if (vkBindImageMemory(m_Device.device(), m_SpotShadowPass.spotShadowMapImage.image, m_SpotShadowPass.spotShadowMapImage.mem, 0))
	{
		throw std::runtime_error("failed to create Point Shadow bind image memory");
	}

	VkImageAspectFlags temp = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	m_Device.TransitionImageLayout(
		layoutCmd,
		m_SpotShadowPass.spotShadowMapImage.image,
		VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	m_Device.endSingleTimeCommands(layoutCmd);

	depthStencilView.image = m_SpotShadowPass.spotShadowMapImage.image;
	if (vkCreateImageView(m_Device.device(), &depthStencilView, nullptr, &m_SpotShadowPass.spotShadowMapImage.view))
	{
		throw std::runtime_error("failed to create Point Shadow depth stencil image viewy");
	}


	VkImageView attachments[2];
	attachments[1] = m_SpotShadowPass.spotShadowMapImage.view;

	VkFramebufferCreateInfo fbufCreateInfo{};
	fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbufCreateInfo.renderPass = m_SpotShadowPass.renderPass;
	fbufCreateInfo.attachmentCount = 2;
	fbufCreateInfo.pAttachments = attachments;
	fbufCreateInfo.width = m_SpotShadowPass.width;
	fbufCreateInfo.height = m_SpotShadowPass.height;
	fbufCreateInfo.layers = 1;

	for (int i = 0; i < MAX_SPOT_LIGHTS; i++)
	{
		attachments[0] = m_SpotShadowPass.imageViews[i];
		if (vkCreateFramebuffer(m_Device.device(), &fbufCreateInfo, nullptr, &m_SpotShadowPass.frameBuffers[i]))
		{
			throw std::runtime_error("failed to create Point Shadow depth stencil image viewy");
		}
	}
}

void SimpleRenderSystem::UpdateSpotShadowMaps(uint32_t lightIndex, FrameInfo frameInfo, GlobalUBO& ubo)
{
	ShadowPassUBO sUbo;
	SpotLight light = ubo.spotLights[lightIndex];
	glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 1000.0f);
	glm::mat4 view = glm::lookAt(glm::vec3(light.position.x, light.position.y, light.position.z), 
		glm::vec3(light.position.x, light.position.y, light.position.z) + glm::vec3(light.direction.x, light.direction.y, light.direction.z), 
		glm::vec3(0.0f, 0.0f, 1.0f));
	sUbo.lightProjection = proj * view;
	m_SpotShadowLightProjectionsUBO.lightProjections[lightIndex] = sUbo.lightProjection;

	m_SpotShadowPassBuffer->writeToIndex(&sUbo, lightIndex);
	m_SpotShadowPassBuffer->flushIndex(lightIndex);

	VkClearValue clearValues[2];
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = m_SpotShadowPass.renderPass;
	renderPassBeginInfo.framebuffer = m_SpotShadowPass.frameBuffers[lightIndex];
	renderPassBeginInfo.renderArea.extent.width = m_SpotShadowPass.width;
	renderPassBeginInfo.renderArea.extent.height = m_SpotShadowPass.height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;


	// Render scene from cube face's point of view
	vkCmdBeginRenderPass(frameInfo.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	m_SpotShadowPassPipeline->bind(frameInfo.commandBuffer);

	std::vector<VkDescriptorSet> globSet = { frameInfo.globalDescriptorSet, m_SpotShadowPassDescriptorSet };
	std::vector<uint32_t> dynamicOffset = { static_cast<uint32_t>(lightIndex * m_SpotShadowPassBuffer->getAlignmentSize()) };
	vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_SpotShadowPassPipelineLayout, 0, globSet.size(), globSet.data(), dynamicOffset.size(), dynamicOffset.data());


	RenderGameObjects(frameInfo.commandBuffer, m_SpotShadowPassPipelineLayout, PushConstantType::SPOTSHADOW, globSet.size(), false);

	vkCmdEndRenderPass(frameInfo.commandBuffer);
}

void SimpleRenderSystem::RenderSpotShadowPass(FrameInfo frameInfo, GlobalUBO& ubo)
{
	PROFILE_FUNCTION();
	VkViewport viewport{};
	viewport.width = (float)m_SpotShadowPass.width;
	viewport.height = (float)m_SpotShadowPass.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(frameInfo.commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.extent.width = m_SpotShadowPass.width;
	scissor.extent.height = m_SpotShadowPass.height;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	vkCmdSetScissor(frameInfo.commandBuffer, 0, 1, &scissor);

	for (uint32_t i = 0; i < ubo.numOfActiveSpotLights; i++)
	{
		m_SpotLightIndex = i;
		UpdateSpotShadowMaps(i, frameInfo, ubo);
	}
}