#pragma once

#include "Device.h"

#include <string>
#include <vector>

struct PipelineConfigInfo
{
	struct PipelineConfigInfo(const PipelineConfigInfo&) = delete;
	PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;
	PipelineConfigInfo() = default;

	std::vector<VkVertexInputBindingDescription> bindingDescription{};
	std::vector<VkVertexInputAttributeDescription> attributeDescription{};
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
	VkPipelineViewportStateCreateInfo viewportInfo;
	VkPipelineRasterizationStateCreateInfo rasterizationInfo;
	VkPipelineMultisampleStateCreateInfo multisampleInfo;
	VkPipelineColorBlendAttachmentState colorBlendAttachment;
	VkPipelineColorBlendStateCreateInfo colorBlendInfo;
	VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
	std::vector<VkDynamicState> dynamicStateEnables;
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo;
	VkPipelineLayout pipelineLayout = nullptr;
	VkRenderPass renderPass = nullptr;
	uint32_t subpass = 0;
};

class Pipeline
{
public:
	Pipeline(
		Device& device,
		const std::string& vertexPath, 
		const std::string& fragPath,
		const PipelineConfigInfo& configInfo);
	~Pipeline();

	Pipeline(const Pipeline&) = delete;
	Pipeline &operator=(const Pipeline&) = delete;
	Pipeline() = default;

	void bind(VkCommandBuffer commandBuffer);
	static void DefaultPipelineConfigInfo(PipelineConfigInfo& configInfo);
	static void EnableAlphaBlending(PipelineConfigInfo& configInfo);

private:
	
	static std::vector<char> ReadFile(const std::string& filePath);

	void CreateShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);

	Device& m_Device;
	VkPipeline m_GraphicsPipeline;
	VkShaderModule m_VertexShaderModule;
	VkShaderModule m_FragmentShaderModule;
};