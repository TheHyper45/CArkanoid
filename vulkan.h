#ifndef VULKAN_H
#define VULKAN_H

#include <stdbool.h>
#include <vulkan/vulkan.h>
#include "quit.h"

#define VK_CHECK(x,...)\
	do\
	{\
		VkResult _result = (x);\
		if(_result)\
		{\
			__VA_ARGS__;\
			AbortApplication("Function call %s returned %s.",#x,VkResultToString(_result));\
		}\
	}\
	while(0)


bool FindMemoryTypeIndex(VkPhysicalDevice physicalDevice,VkMemoryPropertyFlags memoryProperties,uint32_t memoryTypeBits,uint32_t* outMemoryTypeIndex);
const char* VkResultToString(VkResult result);

#endif