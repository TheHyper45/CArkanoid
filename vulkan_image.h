#ifndef VULKAN_IMAGE_H
#define VULKAN_IMAGE_H

#include <stdbool.h>
#include "vulkan.h"

typedef struct RenderingImage
{
	VkImage image;
	VkDeviceMemory memory;
	VkImageView imageView;
	VkDeviceSize size;
	VkExtent2D imageExtent;
	VkImageUsageFlags imageUsage;
} RenderingImage;

bool CreateRenderingImage(VkDevice device,VkPhysicalDevice physicalDevice,VkExtent2D imageExtent,VkImageUsageFlags imageUsage,RenderingImage* outImage);
void DestroyRenderingImage(VkDevice device,RenderingImage image);
bool FillImageTexels(VkDevice device,VkPhysicalDevice physicalDevice,VkCommandPool transferCommandPool,VkQueue transferQueue,const uint8_t* texels,RenderingImage image);

#endif