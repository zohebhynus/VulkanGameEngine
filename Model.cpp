//#include "Model.h"
//
//
//
//Model::~Model()
//{
//}
//
//void Model::Bind(VkCommandBuffer commandBuffer)
//{
//	VkBuffer buffers[] = { m_VertexBuffer->getBuffer() };
//	VkDeviceSize offsets[] = { 0 };
//	vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
//
//	if (m_HasIndexBuffer)
//	{
//		vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
//	}
//}
//
//void Model::Draw(VkCommandBuffer commandBuffer, VkDescriptorSet globalDescriptorSet, VkPipelineLayout pipelineLayout)
//{
//	for (auto& primitive : m_Primitives)
//	{
//		if (m_HasIndexBuffer)
//		{
//			std::vector<VkDescriptorSet> sets{ globalDescriptorSet, primitive.material.descriptorSet };
//			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, sets.size(), sets.data(), 0, nullptr);
//			vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, primitive.firstVertex, 0);
//		}
//		else
//		{
//			vkCmdDraw(commandBuffer, primitive.vertexCount, 1, 0, 0);
//		}
//	}
//}
//
//void Model::CreateVertexBuffers(const std::vector<Vertex>& vertices)
//{
//	uint32_t vertexCount = static_cast<uint32_t>(vertices.size());
//	assert(vertexCount >= 3 && "Vertex count must be at least 3");
//	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
//	uint32_t vertexSize = sizeof(vertices[0]);
//
//	Buffer stagingBuffer
//	{
//		m_Device,
//		vertexSize,
//		vertexCount,
//		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
//	};
//	stagingBuffer.map();
//	stagingBuffer.writeToBuffer((void*)vertices.data());
//
//	m_VertexBuffer = std::make_unique<Buffer>(
//		m_Device,
//		vertexSize,
//		vertexCount,
//		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
//		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
//
//	m_Device.copyBuffer(stagingBuffer.getBuffer(), m_VertexBuffer->getBuffer(), bufferSize);
//}
//
//void Model::CreateIndexBuffer(const std::vector<uint32_t>& indices)
//{
//	uint32_t indexCount = static_cast<uint32_t>(indices.size());
//	m_HasIndexBuffer = indexCount > 0;
//	if (!m_HasIndexBuffer)
//	{
//		return;
//	}
//	VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
//	uint32_t  indexSize = sizeof(indices[0]);
//
//	Buffer stagingBuffer
//	{
//		m_Device,
//		indexSize,
//		indexCount,
//		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
//	};
//
//	stagingBuffer.map();
//	stagingBuffer.writeToBuffer((void*)indices.data());
//
//	m_IndexBuffer = std::make_unique<Buffer>(
//		m_Device,
//		indexSize,
//		indexCount,
//		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
//		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
//
//	m_Device.copyBuffer(stagingBuffer.getBuffer(), m_IndexBuffer->getBuffer(), bufferSize);
//}
//
//std::vector<VkVertexInputBindingDescription> Model::Vertex::getBindingDescriptions()
//{
//	std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
//	bindingDescriptions[0].binding = 0;
//	bindingDescriptions[0].stride = sizeof(Vertex);
//	bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
//
//	return bindingDescriptions;
//}
//
//std::vector<VkVertexInputAttributeDescription> Model::Vertex::getAttributeDescriptions()
//{
//	std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
//
//	attributeDescriptions.push_back({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT , offsetof(Vertex, position) });
//	attributeDescriptions.push_back({ 1, 0, VK_FORMAT_R32G32B32_SFLOAT , offsetof(Vertex, color) });
//	attributeDescriptions.push_back({ 2, 0, VK_FORMAT_R32G32B32_SFLOAT , offsetof(Vertex, normal) });
//	attributeDescriptions.push_back({ 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT , offsetof(Vertex, tangent) });
//	attributeDescriptions.push_back({ 4, 0, VK_FORMAT_R32G32_SFLOAT , offsetof(Vertex, uv) });
//
//	return attributeDescriptions;
//}