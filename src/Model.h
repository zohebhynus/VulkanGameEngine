#pragma once
#include "Graphics/Buffer.h"
#include "Graphics/Texture.h"
#include "Graphics/Device.h"
#include "Graphics/Descriptor.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm/glm.hpp>

#include <memory>
#include <vector>
#include <filesystem>

class Model
{
public:
	struct Material
	{
		std::shared_ptr<Texture> albedoTexture;
		std::shared_ptr<Texture> emissiveTexture;
		std::shared_ptr<Texture> metallicRoughnessTexture;
		std::shared_ptr<Texture> occlusionTexture;
		std::shared_ptr<Texture> normalTexture;

		glm::vec4 albedoFactor = glm::vec4(1.0f);
		glm::vec4 emissiveFactor = glm::vec4(1.0f);
		float emissiveStrength = 1.0f;
		float metallicFactor = 1.0f;
		float roughnessFactor = 1.0f;

		VkDescriptorSet descriptorSet;
	};

	struct Primitive
	{
		uint32_t firstIndex;
		uint32_t firstVertex;
		uint32_t indexCount;
		uint32_t vertexCount;
		Material material;
	};

	struct Vertex
	{
		glm::vec3 position {};
		glm::vec3 color {};
		glm::vec3 normal {};
		glm::vec4 tangent {};
		glm::vec2 uv {};

		static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
		static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

		bool operator==(const Vertex& other) const
		{
			return
				position == other.position &&
				color == other.color &&
				normal == other.normal &&
				uv == other.uv;
		}
	};

	Model(Device& device, const std::string& filePath, DescriptorSetLayout& materialSetLayout, DescriptorPool& descriptorPool);
	~Model();

	void Bind(VkCommandBuffer commandBuffer);
	void Draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, int setCount, bool renderMaterial);

private:
	void CreateVertexBuffers(const std::vector<Vertex>& vertices);
	void CreateIndexBuffer(const std::vector<uint32_t>& indices);

	Device& m_Device;

	std::unique_ptr<Buffer> m_VertexBuffer;
	std::unique_ptr<Buffer> m_IndexBuffer;
	bool m_HasIndexBuffer = false;

	std::vector<Vertex> m_Vertices;
	std::vector<uint32_t> m_Indices;
	std::vector<Primitive> m_Primitives;
	std::vector<std::shared_ptr<Texture>> m_Textures;
};