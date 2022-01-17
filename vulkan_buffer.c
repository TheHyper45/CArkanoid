#include "vulkan_buffer.h"

#include <string.h>

bool CreateRenderingBuffer(VkDevice device,VkPhysicalDevice physicalDevice,VkDeviceSize size,const void* data,VkMemoryPropertyFlags memoryProperties,VkBufferUsageFlags bufferUsage,RenderingBuffer* outBuffer)
{
	outBuffer->size = size;
	outBuffer->bufferUsage = bufferUsage;
	outBuffer->memoryProperties = memoryProperties;

	VkBufferCreateInfo bufferCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.usage = outBuffer->bufferUsage,
		.size = outBuffer->size
	};
	VkResult result = vkCreateBuffer(device,&bufferCreateInfo,NULL,&outBuffer->buffer);
	if(result != VK_SUCCESS)
	{
		SetError("Function call vkCreateBuffer(device,&bufferCreateInfo,NULL,&outBuffer->buffer) returned %s.",VkResultToString(result));
		return false;
	}

	VkMemoryRequirements memoryRequirements = {0};
	vkGetBufferMemoryRequirements(device,outBuffer->buffer,&memoryRequirements);
	uint32_t memoryTypeIndex = 0;
	if(!FindMemoryTypeIndex(physicalDevice,outBuffer->memoryProperties,memoryRequirements.memoryTypeBits,&memoryTypeIndex))
	{
		DestroyRenderingBuffer(device,*outBuffer);
		return false;
	}

	VkMemoryAllocateInfo memoryAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memoryRequirements.size,
		.memoryTypeIndex = memoryTypeIndex
	};
	result = vkAllocateMemory(device,&memoryAllocateInfo,NULL,&outBuffer->memory);
	if(result != VK_SUCCESS)
	{
		DestroyRenderingBuffer(device,*outBuffer);
		SetError("Function call vkAllocateMemory(device,&memoryAllocateInfo,NULL,&outBuffer->memory) returned %s.",VkResultToString(result));
		return false;
	}
	result = vkBindBufferMemory(device,outBuffer->buffer,outBuffer->memory,0);
	if(result != VK_SUCCESS)
	{
		DestroyRenderingBuffer(device,*outBuffer);
		SetError("Function call vkBindBufferMemory(device,outBuffer->buffer,outBuffer->memory,0) returned %s.",VkResultToString(result));
		return false;
	}

	if(data)
	{
		void* mappedData = NULL;
		result = vkMapMemory(device,outBuffer->memory,0,VK_WHOLE_SIZE,0,&mappedData);
		if(result != VK_SUCCESS)
		{
			DestroyRenderingBuffer(device,*outBuffer);
			SetError("Function call vkMapMemory(device,outBuffer->memory,0,VK_WHOLE_SIZE,0,&mappedData) returned %s.",VkResultToString(result));
			return false;
		}
		memcpy(mappedData,data,outBuffer->size);
		vkUnmapMemory(device,outBuffer->memory);
	}
	return true;
}

void DestroyRenderingBuffer(VkDevice device,RenderingBuffer buffer)
{
	vkDestroyBuffer(device,buffer.buffer,NULL);
	vkFreeMemory(device,buffer.memory,NULL);
}