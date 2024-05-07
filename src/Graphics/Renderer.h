#pragma once

#include "Device.h"
#include "SwapChain.h"
#include "../Window.h"

#include <cassert>
#include <memory>
#include <vector>

class Renderer
{
public:
	Renderer(Window& window, Device& device);
	~Renderer();

	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;

	VkRenderPass GetSwapChainRenderPass() const { return m_SwapChain->getRenderPass(); }
	float GetAspectRatio() const { return m_SwapChain->extentAspectRatio(); }
	bool IsFrameInProgress() const { return isFrameStarted; }
	VkCommandBuffer GetCurrentCommandBuffer() const 
	{
		assert(IsFrameInProgress() && "Cannot get command buffer when frame not in progress");
		return m_CommandBuffers[currentFrameIndex]; 
	}

	int GetFrameIndex() const 
	{
		assert(IsFrameInProgress() && "Cannot get frame index when frame not in progress");
		return currentFrameIndex;
	}

	VkCommandBuffer BeginFrame();
	void EndFrame();

	void BeginSwapChainRenderPass(VkCommandBuffer commandBuffer);
	void EndSwapChainRenderPass(VkCommandBuffer commandBuffer);

private:
	void createCommandBuffers();
	void freeCommandBuffers();
	void recreateSwapChain();

	Window& m_Window;
	Device& m_Device;
	std::unique_ptr<SwapChain> m_SwapChain;
	std::vector<VkCommandBuffer> m_CommandBuffers;

	uint32_t currentImageIndex;
	int currentFrameIndex{0};
	bool isFrameStarted{false};
};