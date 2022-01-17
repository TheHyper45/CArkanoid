#ifndef RENDERER_BUFFER_H
#define RENDERER_BUFFER_H

#include <stdbool.h>
#include "vulkan.h"

typedef struct RenderingBuffer
{
	VkBuffer buffer;
	VkDeviceMemory memory;
	VkDeviceSize size;
	VkBufferUsageFlags bufferUsage;
	VkMemoryPropertyFlags memoryProperties;
} RenderingBuffer;

bool CreateRenderingBuffer(VkDevice device,VkPhysicalDevice physicalDevice,VkDeviceSize size,const void* data,VkMemoryPropertyFlags memoryProperties,VkBufferUsageFlags bufferUsage,RenderingBuffer* outBuffer);
void DestroyRenderingBuffer(VkDevice device,RenderingBuffer buffer);

#endif