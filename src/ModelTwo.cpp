#include "ModelTwo.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm/gtx/hash.hpp>
#include <glm/glm/gtc/type_ptr.hpp>

#define TINYGLTF_IMPLEMENTATION
//#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define STBI_MSC_SECURE_CRT
#include <tiny_gltf/tiny_gltf.h>

#include <iostream>

Model::Model(Device& device, const std::string& filePath, DescriptorSetLayout& materialSetLayout, DescriptorPool& descriptorPool) : m_Device{device}
{
	std::string warn, err;
	tinygltf::TinyGLTF gltfLoader;
	tinygltf::Model gltfModel;

	// Load Model
	if (!gltfLoader.LoadASCIIFromFile(&gltfModel, &err, &warn, filePath))
	{
		throw std::runtime_error("Failed to load gltf file!");
	}

	auto path = std::filesystem::path(filePath);
	for (auto& texture : gltfModel.images)
	{
		m_Textures.push_back(std::make_shared<Texture>(m_Device, path.parent_path().append(texture.uri).generic_string()));
	}

	for (auto& scene : gltfModel.scenes)
	{
		for (size_t i = 0; i < scene.nodes.size(); i++)
		{
			auto& node = gltfModel.nodes[i];
			uint32_t vertexOffset = 0;
			uint32_t indexOffset = 0;

			for (auto& gltfPrimitive : gltfModel.meshes[node.mesh].primitives)
			{
				uint32_t vertexCount = 0;
				uint32_t indexCount = 0;

				const float* positionsBuffer = nullptr;
				const float* normalsBuffer = nullptr;
				const float* tangentsBuffer = nullptr;
				const float* texCoordsBuffer = nullptr;

				if (gltfPrimitive.attributes.find("POSITION") != gltfPrimitive.attributes.end())
				{
					const tinygltf::Accessor& accessor = gltfModel.accessors[gltfPrimitive.attributes.find("POSITION")->second];
					const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
					positionsBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					vertexCount = accessor.count;
				}

				if (gltfPrimitive.attributes.find("NORMAL") != gltfPrimitive.attributes.end())
				{
					const tinygltf::Accessor& accessor = gltfModel.accessors[gltfPrimitive.attributes.find("NORMAL")->second];
					const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
					normalsBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}

				if (gltfPrimitive.attributes.find("TANGENT") != gltfPrimitive.attributes.end())
				{
					const tinygltf::Accessor& accessor = gltfModel.accessors[gltfPrimitive.attributes.find("TANGENT")->second];
					const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
					tangentsBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}

				if (gltfPrimitive.attributes.find("TEXCOORD_0") != gltfPrimitive.attributes.end())
				{
					const tinygltf::Accessor& accessor = gltfModel.accessors[gltfPrimitive.attributes.find("TEXCOORD_0")->second];
					const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
					texCoordsBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}

				for (size_t v = 0; v < vertexCount; v++)
				{
					Vertex vertex{};
					vertex.position = glm::make_vec3(&positionsBuffer[v * 3]);
					vertex.color = glm::vec3(1.0f);
					vertex.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
					vertex.tangent = glm::vec4(tangentsBuffer ? glm::make_vec4(&tangentsBuffer[v * 4]) : glm::vec4(0.0f));
					vertex.uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec2(0.0f);

					m_Vertices.push_back(vertex);
				}

				{
					const tinygltf::Accessor& accessor = gltfModel.accessors[gltfPrimitive.indices];
					const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
					const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];

					indexCount += static_cast<uint32_t>(accessor.count);

					switch (accessor.componentType)
					{
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
						const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
						for (size_t index = 0; index < accessor.count; index++) {
							m_Indices.push_back(buf[index]);
						}
						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
						const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
						for (size_t index = 0; index < accessor.count; index++) {
							m_Indices.push_back(buf[index]);
						}
						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
						const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
						for (size_t index = 0; index < accessor.count; index++) {
							m_Indices.push_back(buf[index]);
						}
						break;
					}
					default:
						std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
						return;
					}
				}

				std::shared_ptr<Texture> defaultTexture = std::make_shared<Texture>(m_Device, "Assets/Textures/white.png");

				Material material{};
				if (gltfPrimitive.material != -1)
				{
					tinygltf::Material& gltfPrimitiveMaterial = gltfModel.materials[gltfPrimitive.material];

					if (gltfPrimitiveMaterial.pbrMetallicRoughness.baseColorTexture.index != -1)
					{
						uint32_t textureIndex = gltfPrimitiveMaterial.pbrMetallicRoughness.baseColorTexture.index;
						uint32_t imageIndex = gltfModel.textures[textureIndex].source;
						material.albedoTexture = m_Textures[imageIndex];
					}
					else
					{
						material.albedoTexture = defaultTexture;
					}

					if (gltfPrimitiveMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index != -1)
					{
						uint32_t textureIndex = gltfPrimitiveMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
						uint32_t imageIndex = gltfModel.textures[textureIndex].source;
						material.metallicRoughnessTexture = m_Textures[imageIndex];
					}
					else
					{
						material.metallicRoughnessTexture = defaultTexture;
					}

					if (gltfPrimitiveMaterial.normalTexture.index != -1)
					{
						uint32_t textureIndex = gltfPrimitiveMaterial.normalTexture.index;
						uint32_t imageIndex = gltfModel.textures[textureIndex].source;
						material.normalTexture = m_Textures[imageIndex];
					}
					else
					{
						material.normalTexture = defaultTexture;
					}
				}
				else
				{
					material.albedoTexture = defaultTexture;
					material.normalTexture = defaultTexture;
					material.metallicRoughnessTexture = defaultTexture;
				}

				VkDescriptorImageInfo albedoInfo = {};
				albedoInfo.sampler = material.albedoTexture->GetSampler();
				albedoInfo.imageView = material.albedoTexture->GetImageView();
				albedoInfo.imageLayout = material.albedoTexture->GetImageLayout();

				VkDescriptorImageInfo normalInfo = {};
				normalInfo.sampler = material.normalTexture->GetSampler();
				normalInfo.imageView = material.normalTexture->GetImageView();
				normalInfo.imageLayout = material.normalTexture->GetImageLayout();

				VkDescriptorImageInfo metallicRoughnessInfo = {};
				metallicRoughnessInfo.sampler = material.metallicRoughnessTexture->GetSampler();
				metallicRoughnessInfo.imageView = material.metallicRoughnessTexture->GetImageView();
				metallicRoughnessInfo.imageLayout = material.metallicRoughnessTexture->GetImageLayout();

				DescriptorWriter(materialSetLayout, descriptorPool)
					.writeImage(0, &albedoInfo)
					.writeImage(1, &normalInfo)
					.writeImage(2, &metallicRoughnessInfo)
					.build(material.descriptorSet);

				Primitive primitive{};
				primitive.firstIndex = indexOffset;
				primitive.firstVertex = vertexOffset;
				primitive.indexCount = indexCount;
				primitive.vertexCount = vertexCount;
				primitive.material = material;
				
				m_Primitives.push_back(primitive);

				vertexOffset += vertexCount;
				indexOffset += indexCount;
			}
		}
		CreateVertexBuffers(m_Vertices);
		CreateIndexBuffer(m_Indices);
	}
}

Model::~Model()
{
}

void Model::Bind(VkCommandBuffer commandBuffer)
{
	VkBuffer buffers[] = { m_VertexBuffer->getBuffer() };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

	if (m_HasIndexBuffer)
	{
		vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
	}
}

void Model::Draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, int setCount, bool renderMaterial)
{
	for (auto& primitive : m_Primitives)
	{
		if (m_HasIndexBuffer)
		{
			//sets.push_back(primitive.material.descriptorSet);
			// my god
			if (renderMaterial)
			{
				std::vector<VkDescriptorSet> sets = { primitive.material.descriptorSet };
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, setCount, sets.size(), sets.data(), 0, nullptr);
			}
			vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, primitive.firstVertex, 0);
		}
		else
		{
			vkCmdDraw(commandBuffer, primitive.vertexCount, 1, 0, 0);
		}
	}
}

void Model::CreateVertexBuffers(const std::vector<Vertex>& vertices)
{
	uint32_t vertexCount = static_cast<uint32_t>(vertices.size());
	assert(vertexCount >= 3 && "Vertex count must be at least 3");
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
	uint32_t vertexSize = sizeof(vertices[0]);

	Buffer stagingBuffer
	{
		m_Device,
		vertexSize,
		vertexCount,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	};
	stagingBuffer.map();
	stagingBuffer.writeToBuffer((void*)vertices.data());

	m_VertexBuffer = std::make_unique<Buffer>(
		m_Device,
		vertexSize,
		vertexCount,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	m_Device.copyBuffer(stagingBuffer.getBuffer(), m_VertexBuffer->getBuffer(), bufferSize);
}

void Model::CreateIndexBuffer(const std::vector<uint32_t>& indices)
{
	uint32_t indexCount = static_cast<uint32_t>(indices.size());
	m_HasIndexBuffer = indexCount > 0;
	if (!m_HasIndexBuffer)
	{
		return;
	}
	VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
	uint32_t  indexSize = sizeof(indices[0]);

	Buffer stagingBuffer
	{
		m_Device,
		indexSize,
		indexCount,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	};

	stagingBuffer.map();
	stagingBuffer.writeToBuffer((void*)indices.data());

	m_IndexBuffer = std::make_unique<Buffer>(
		m_Device,
		indexSize,
		indexCount,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	m_Device.copyBuffer(stagingBuffer.getBuffer(), m_IndexBuffer->getBuffer(), bufferSize);
}

std::vector<VkVertexInputBindingDescription> Model::Vertex::getBindingDescriptions()
{
	std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
	bindingDescriptions[0].binding = 0;
	bindingDescriptions[0].stride = sizeof(Vertex);
	bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription> Model::Vertex::getAttributeDescriptions()
{
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

	attributeDescriptions.push_back({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT , offsetof(Vertex, position) });
	attributeDescriptions.push_back({ 1, 0, VK_FORMAT_R32G32B32_SFLOAT , offsetof(Vertex, color) });
	attributeDescriptions.push_back({ 2, 0, VK_FORMAT_R32G32B32_SFLOAT , offsetof(Vertex, normal) });
	attributeDescriptions.push_back({ 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT , offsetof(Vertex, tangent) });
	attributeDescriptions.push_back({ 4, 0, VK_FORMAT_R32G32_SFLOAT , offsetof(Vertex, uv) });

	return attributeDescriptions;
}
