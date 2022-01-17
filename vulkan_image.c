#include "vulkan_image.h"

#include "vulkan_buffer.h"

bool CreateRenderingImage(VkDevice device,VkPhysicalDevice physicalDevice,VkExtent2D imageExtent,VkImageUsageFlags imageUsage,RenderingImage* outImage)
{
	outImage->size = (VkDeviceSize)imageExtent.width * imageExtent.height * 4;
	outImage->imageExtent = imageExtent;
	outImage->imageUsage = imageUsage;
	
	VkImageCreateInfo imageCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.arrayLayers = 1,
		.mipLevels = 1,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.imageType = VK_IMAGE_TYPE_2D,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.usage = outImage->imageUsage,
		.extent = {outImage->imageExtent.width,outImage->imageExtent.height,1}
	};

	VkResult result = vkCreateImage(device,&imageCreateInfo,NULL,&outImage->image);
	if(result != VK_SUCCESS)
	{
		SetError("Function call vkCreateImage(device,&imageCreateInfo,NULL,&outImage->image) returned %s.",VkResultToString(result));
		return false;
	}

	VkMemoryRequirements memoryRequirements = {0};
	vkGetImageMemoryRequirements(device,outImage->image,&memoryRequirements);
	uint32_t memoryTypeIndex = 0;
	if(!FindMemoryTypeIndex(physicalDevice,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,memoryRequirements.memoryTypeBits,&memoryTypeIndex))
	{
		DestroyRenderingImage(device,*outImage);
		return false;
	}

	VkMemoryAllocateInfo memoryAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memoryRequirements.size,
		.memoryTypeIndex = memoryTypeIndex
	};
	result = vkAllocateMemory(device,&memoryAllocateInfo,NULL,&outImage->memory);
	if(result != VK_SUCCESS)
	{
		DestroyRenderingImage(device,*outImage);
		SetError("Function call vkAllocateMemory(device,&memoryAllocateInfo,NULL,&outImage->memory) returned %s.",VkResultToString(result));
		return false;
	}
	result = vkBindImageMemory(device,outImage->image,outImage->memory,0);
	if(result != VK_SUCCESS)
	{
		DestroyRenderingImage(device,*outImage);
		SetError("Function call vkBindImageMemory(device,outImage->image,outImage->memory,0) returned %s.",VkResultToString(result));
		return false;
	}

	VkImageViewCreateInfo imageViewCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = outImage->image,
		.format = imageCreateInfo.format,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.components = {
			.r = VK_COMPONENT_SWIZZLE_IDENTITY,
			.g = VK_COMPONENT_SWIZZLE_IDENTITY,
			.b = VK_COMPONENT_SWIZZLE_IDENTITY,
			.a = VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.layerCount = 1,
			.levelCount = 1
		}
	};
	result = vkCreateImageView(device,&imageViewCreateInfo,NULL,&outImage->imageView);
	if(result != VK_SUCCESS)
	{
		DestroyRenderingImage(device,*outImage);
		SetError("Function call vkCreateImageView(device,&imageViewCreateInfo,NULL,&outImage->imageView) returned %s.",VkResultToString(result));
		return false;
	}
	return true;
}

void DestroyRenderingImage(VkDevice device,RenderingImage image)
{
	vkDestroyImageView(device,image.imageView,NULL);
	vkDestroyImage(device,image.image,NULL);
	vkFreeMemory(device,image.memory,NULL);
}

bool FillImageTexels(VkDevice device,VkPhysicalDevice physicalDevice,VkCommandPool transferCommandPool,VkQueue transferQueue,const uint8_t* texels,RenderingImage image)
{
	RenderingBuffer stagingBuffer = {0};
	if(!CreateRenderingBuffer(device,physicalDevice,image.size,texels,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,&stagingBuffer))
	{
		return false;
	}

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandPool = transferCommandPool,
		.commandBufferCount = 1
	};
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	VkResult result = vkAllocateCommandBuffers(device,&commandBufferAllocateInfo,&commandBuffer);
	if(result != VK_SUCCESS)
	{
		DestroyRenderingBuffer(device,stagingBuffer);
		SetError("Function call vkAllocateCommandBuffers(device,&commandBufferAllocateInfo,&commandBuffer) returned %s.",VkResultToString(result));
		return false;
	}

	VkCommandBufferBeginInfo commandBufferBeginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	result = vkBeginCommandBuffer(commandBuffer,&commandBufferBeginInfo);
	if(result != VK_SUCCESS)
	{
		vkFreeCommandBuffers(device,transferCommandPool,1,&commandBuffer);
		DestroyRenderingBuffer(device,stagingBuffer);
		SetError("Function call vkBeginCommandBuffer(commandBuffer,&commandBufferBeginInfo) returned %s.",VkResultToString(result));
		return false;
	}

	VkImageMemoryBarrier barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.image = image.image,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.layerCount = 1,
			.levelCount = 1
		}
	};
	vkCmdPipelineBarrier(commandBuffer,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,1,&barrier);

	VkBufferImageCopy bufferImageCopy = {
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageOffset = {0,0,0},
		.imageExtent = {image.imageExtent.width,image.imageExtent.height,1},
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.layerCount = 1
		}
	};

	vkCmdCopyBufferToImage(commandBuffer,stagingBuffer.buffer,image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&bufferImageCopy);

	barrier.srcAccessMask = barrier.dstAccessMask;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.oldLayout = barrier.newLayout;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	vkCmdPipelineBarrier(commandBuffer,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,NULL,0,NULL,1,&barrier);

	result = vkEndCommandBuffer(commandBuffer);
	if(result != VK_SUCCESS)
	{
		vkFreeCommandBuffers(device,transferCommandPool,1,&commandBuffer);
		DestroyRenderingBuffer(device,stagingBuffer);
		SetError("Function call vkEndCommandBuffer(commandBuffer) returned %s.",VkResultToString(result));
		return false;
	}

	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &commandBuffer
	};
	result = vkQueueSubmit(transferQueue,1,&submitInfo,VK_NULL_HANDLE);
	if(result != VK_SUCCESS)
	{
		vkFreeCommandBuffers(device,transferCommandPool,1,&commandBuffer);
		DestroyRenderingBuffer(device,stagingBuffer);
		SetError("Function call vkQueueSubmit(transferQueue,1,&submitInfo,VK_NULL_HANDLE) returned %s.",VkResultToString(result));
		return false;
	}
	result = vkQueueWaitIdle(transferQueue);
	if(result != VK_SUCCESS)
	{
		vkFreeCommandBuffers(device,transferCommandPool,1,&commandBuffer);
		DestroyRenderingBuffer(device,stagingBuffer);
		SetError("Function call vkQueueWaitIdle(transferQueue) returned %s.",VkResultToString(result));
		return false;
	}

	vkFreeCommandBuffers(device,transferCommandPool,1,&commandBuffer);
	DestroyRenderingBuffer(device,stagingBuffer);
	return true;
}