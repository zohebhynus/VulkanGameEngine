#pragma once

#include "../Device.h"
#include "../FrameInfo.h"
#include "../../GameObject.h"
#include "../Pipeline.h"
#include "../Camera.h"
#include "../../Components.h"

#include <memory>
#include <vector>

class PointLightRenderSystem : public System
{
public:
	PointLightRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
	~PointLightRenderSystem();

	PointLightRenderSystem(const PointLightRenderSystem&) = delete;
	PointLightRenderSystem& operator=(const PointLightRenderSystem&) = delete;

	void Update(FrameInfo& frameInfo, GlobalUBO& ubo);
	void Render(FrameInfo& frameInfo, GlobalUBO& globalUBO);

	glm::vec3 point = { 1.0f, -6.0f, 0.0f };
private:
	void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
	void createPipeline(VkRenderPass renderpass);

	Device& m_Device;
	std::unique_ptr<Pipeline> m_Pipeline;
	VkPipelineLayout m_PipelineLayout;

};