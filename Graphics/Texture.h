#pragma once
#include "Device.h"

class Texture
{
public:
	Texture(Device& device, const std::string& filePath);
	~Texture();

	VkSampler GetSampler() { return m_Sampler; }
	VkImageView GetImageView() { return m_ImageView; }
	VkImageLayout GetImageLayout() { return m_ImageLayout; }
	VkDescriptorImageInfo getImageInfo() const { return m_DescriptorImageInfo; }

private:
	void TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
	void GenerateMipmaps();
	void UpdateDescriptor();

	Device& m_Device;

	int m_Width, m_Height, m_MipLevels;

	VkImage m_Image;
	VkDeviceMemory m_ImageMemory;

	VkSampler m_Sampler;
	VkImageView m_ImageView;
	VkImageLayout m_ImageLayout;

	VkFormat m_ImageFormat;

	VkDescriptorImageInfo m_DescriptorImageInfo {};
};