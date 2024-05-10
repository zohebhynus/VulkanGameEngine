#include "Renderer.h"
#include "../Instrumentation.h"

#include <array>
#include <cassert>
#include <stdexcept>

Renderer::Renderer(Window& window, Device& device) :m_Window(window), m_Device(device)
{
	recreateSwapChain();
	createCommandBuffers();
}

Renderer::~Renderer()
{
	freeCommandBuffers();
}

VkCommandBuffer Renderer::BeginFrame()
{
	assert(!isFrameStarted && "Cannot call Begin Frame while already in progress");

	auto result = m_SwapChain->acquireNextImage(&currentImageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		recreateSwapChain();
		return nullptr;
	}
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("Failed to acquire swap chain image");
	}
	isFrameStarted = true;

	auto commandBuffer = GetCurrentCommandBuffer();
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo))
	{
		throw std::runtime_error("Failed to begin recording command buffer");
	}
	return commandBuffer;
}

void Renderer::EndFrame()
{
	PROFILE_FUNCTION();
	assert(isFrameStarted && "Cannot call End Frame while frame is not in progress");

	auto commandBuffer = GetCurrentCommandBuffer();
	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to recording command buffer");
	}

	auto result = m_SwapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_Window.WasWindowResized())
	{
		m_Window.ResetWindowResizedFlag();
		recreateSwapChain();
	}
	else if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to present swap chain image");

	}
	isFrameStarted = false;
	currentFrameIndex = (currentFrameIndex + 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;
}

void Renderer::BeginSwapChainRenderPass(VkCommandBuffer commandBuffer)
{
	assert(isFrameStarted && "Cannot call Begin Swap Chain Render Pass while frame is not in progress");
	assert(commandBuffer == GetCurrentCommandBuffer() && "Can't begin render pass on a command buffer from a different frame");

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = m_SwapChain->getRenderPass();
	renderPassInfo.framebuffer = m_SwapChain->getFrameBuffer(currentImageIndex);
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = m_SwapChain->getSwapChainExtent();

	std::vector<VkClearValue> clearValues(2);
	clearValues[0].color = { 0.1f, 0.1f, 0.1f, 1.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };
	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_SwapChain->getSwapChainExtent().width);
	viewport.height = static_cast<float>(m_SwapChain->getSwapChainExtent().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor{ {0, 0}, m_SwapChain->getSwapChainExtent() };
	vkCmdSetViewport(commandBuffer, 0, 1, & viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

}

void Renderer::EndSwapChainRenderPass(VkCommandBuffer commandBuffer)
{
	PROFILE_FUNCTION();
	assert(isFrameStarted && "Cannot call End Swap Chain Render Pass while frame is not in progress");
	assert(commandBuffer == GetCurrentCommandBuffer() && "Can't End render pass on a command buffer from a different frame");

	vkCmdEndRenderPass(commandBuffer);
}

void Renderer::createCommandBuffers()
{
	m_CommandBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandPool = m_Device.getCommandPool();
	allocateInfo.commandBufferCount = static_cast<uint32_t>(m_CommandBuffers.size());

	if (vkAllocateCommandBuffers(m_Device.device(), &allocateInfo, m_CommandBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate command buffer");
	}
}

void Renderer::freeCommandBuffers()
{
	vkFreeCommandBuffers(m_Device.device(), m_Device.getCommandPool(), static_cast<uint32_t>(m_CommandBuffers.size()), m_CommandBuffers.data());
	m_CommandBuffers.clear();
}

void Renderer::recreateSwapChain()
{
	auto extent = m_Window.GetExtent();
	while (extent.width == 0 || extent.height == 0)
	{
		extent = m_Window.GetExtent();
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(m_Device.device());
	if (m_SwapChain == nullptr)
	{
		m_SwapChain = std::make_unique<SwapChain>(m_Device, extent);
	}
	else
	{
		std::shared_ptr<SwapChain> oldSwapChain = std::move(m_SwapChain);
		m_SwapChain = std::make_unique<SwapChain>(m_Device, extent, oldSwapChain);

		if (!oldSwapChain->compareSwapFormats(*m_SwapChain.get()))
		{
			throw std::runtime_error("Swap chain image(or depth) format has changed!");
		}
	}
	// we'll come back to this
}
