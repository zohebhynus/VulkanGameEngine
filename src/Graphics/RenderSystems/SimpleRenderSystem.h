#pragma once

#include "../Device.h"
#include "../FrameInfo.h"
#include "../../GameObject.h"
#include "../Pipeline.h"
#include "../Camera.h"
#include "../../Components.h"
#include "../Descriptor.h"
#include "../SwapChain.h"

#include <memory>
#include <vector>

class SimpleRenderSystem : public System
{
public:

	typedef enum PushConstantType
	{
		MAIN = 0,
		POINTSHADOW = 1,
		SPOTSHADOW = 2
	};

	struct ShadowFrameBufferAttachment {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
	};
	struct ShadowPass {
		uint32_t width, height;
		VkFramebuffer frameBuffer;
		ShadowFrameBufferAttachment shadowMapImage;
		VkRenderPass renderPass;
		VkSampler shadowMapSampler;
		VkDescriptorImageInfo descriptor;
	};

	struct ShadowPassUBO
	{
		glm::mat4 lightProjection;
	};

	struct PointShadowPass {
		uint32_t width, height;
		std::array<std::array<VkFramebuffer, 6>, MAX_POINT_LIGHTS> frameBuffers;
		ShadowFrameBufferAttachment pointShadowMapImage;
		VkRenderPass renderPass;
		VkSampler pointShadowMapSampler;
		VkDescriptorImageInfo descriptor;
	};

	struct TextureArray {
		uint32_t width, height;
		ShadowFrameBufferAttachment cubeMapImage;
		VkSampler cubeMapSampler;
	};

	struct PointShadowPassViewMatrixUBO
	{
		std::array<glm::mat4, 6> faceViewMatrix;
	};


	struct SpotShadowPass
	{
		uint32_t width, height;
		std::array<VkFramebuffer, MAX_SPOT_LIGHTS> frameBuffers;
		std::array<VkImageView, MAX_SPOT_LIGHTS> imageViews;
		ShadowFrameBufferAttachment spotShadowMapImage;
		VkRenderPass renderPass;
		VkSampler sampler;
	};

	struct SpotShadowLightProjectionsUBO
	{
		std::array<glm::mat4, MAX_SPOT_LIGHTS> lightProjections;
	};

	SimpleRenderSystem(
		Device& device,
		VkRenderPass renderPass, 
		std::vector<VkDescriptorSetLayout> setLayouts,
		DescriptorPool& descriptorPool);
	~SimpleRenderSystem();

	SimpleRenderSystem(const SimpleRenderSystem&) = delete;
	SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;

	void RenderShadowPass(FrameInfo frameInfo, GlobalUBO& globalUBO);
	void RenderPointShadowPass(FrameInfo frameInfo, GlobalUBO& globalUBO);
	void RenderSpotShadowPass(FrameInfo frameInfo, GlobalUBO& globalUBO);
	void RenderMainPass(FrameInfo frameInfo);

	void RenderGameObjects(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, PushConstantType type, int setCount, bool renderMaterial = true);

private:
	void createPipelineLayout(std::vector<VkDescriptorSetLayout> setLayouts, DescriptorPool& descriptorPool);
	void createPipeline(VkRenderPass renderpass);

	void PrepareShadowPassUBO();

	void PrepareShadowPassRenderpass();
	void PrepareShadowPassFramebuffer();
	void UpdateShadowPassBuffer(GlobalUBO& globalUBO);

	void PreparePointShadowCubeMaps();
	void PreparePointShadowPassRenderPass();
	void PreparePointShadowPassFramebuffers();

	void updateCubeFace(uint32_t faceIndex, FrameInfo frameInfo, GlobalUBO& ubo);

	void PrepareSpotShadowMaps();
	void PrepareSpotShadowPassRenderPass();
	void PrepareSpotShadowPassFramebuffers();

	void UpdateSpotShadowMaps(uint32_t lightIndex, FrameInfo frameInfo, GlobalUBO& ubo);

	Device& m_Device;
	// Main Pipeline variables
	std::unique_ptr<Pipeline> m_MainPipeline;
	VkPipelineLayout m_MainPipelineLayout;

	VkDescriptorSet m_ShadowMapDescriptorSet;

	TextureArray m_PointShadowCubeMaps{};
	VkDescriptorSet m_PointShadowMapDescriptorSet;

	TextureArray m_SpotShadowMaps{};
	VkDescriptorSet m_SpotShadowMapDescriptorSet;

	SpotShadowLightProjectionsUBO m_SpotShadowLightProjectionsUBO{};
	std::unique_ptr<Buffer> m_SpotShadowLightProjectionsBuffer;
	VkDescriptorSet m_SpotShadowLightProjectionsDescriptorSet;

	// Directional Shadow variables
	std::unique_ptr<Pipeline> m_ShadowPassPipeline;
	VkPipelineLayout m_ShadowPassPipelineLayout;

	const VkFormat m_ShadowPassImageFormat{ VK_FORMAT_D16_UNORM };
	const uint32_t m_ShadowMapSize{ 2048 };

	ShadowPassUBO m_ShadowPassUBO;
	std::unique_ptr<Buffer> m_ShadowPassBuffer;
	VkDescriptorSet m_ShadowPassDescriptorSet;

	ShadowPass m_ShadowPass{};

	//Point Shadow variables
	std::unique_ptr<Pipeline> m_PointShadowPassPipeline;
	VkPipelineLayout m_PointShadowPassPipelineLayout;

	const uint32_t m_PointShadowMapSize{ 1024 };
	const VkFormat m_PointShadowPassImageFormat{ VK_FORMAT_R32_SFLOAT };
	VkFormat m_PointShadowPassDepthFormat{ VK_FORMAT_UNDEFINED };

	PointShadowPassViewMatrixUBO m_PointShadowPassUBO {};
	std::unique_ptr<Buffer> m_PointShadowPassBuffer;
	VkDescriptorSet m_PointShadowPassDescriptorSet;

	PointShadowPass m_PointShadowPass{};
	std::array<std::array<VkImageView, 6>, MAX_POINT_LIGHTS> m_PointShadowCubeMapImageViews{};
	int m_PointLightCount = 0;
	int m_FaceCount = 0;

	//Spot Shadow variables
	std::unique_ptr<Pipeline> m_SpotShadowPassPipeline;
	VkPipelineLayout m_SpotShadowPassPipelineLayout;

	const uint32_t m_SpotShadowMapSize{ 1024 };
	const VkFormat m_SpotShadowPassImageFormat{ VK_FORMAT_R32_SFLOAT };
	VkFormat m_SpotShadowPassDepthFormat{ VK_FORMAT_UNDEFINED };

	ShadowPassUBO m_SpotShadowPassUBO;
	std::unique_ptr<Buffer> m_SpotShadowPassBuffer;
	VkDescriptorSet m_SpotShadowPassDescriptorSet;

	SpotShadowPass m_SpotShadowPass{};

	int m_SpotLightIndex = 0;
};