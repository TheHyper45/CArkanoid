#include "renderer.h"

#include <string.h>
#include <stdlib.h>
#include <SDL_image.h>
#include <shaderc/shaderc.h>
#include "vulkan.h"
#include "engine.h"
#include "vulkan_image.h"
#include "vulkan_buffer.h"

typedef struct TransformationMatrix
{
	Mat4 matrix;
} TransformationMatrix;

typedef struct ImageData
{
	RenderingImage image;
	VkDescriptorSet descriptorSet;
} ImageData;

typedef struct Renderer
{
	VkPhysicalDevice physicalDevice;
	uint32_t graphicsQueueFamilyIndex;
	VkDevice device;
	VkQueue graphicsQueue;
	uint32_t swapchainImageCount;
	VkExtent2D swapchainImageExtent;
	VkSurfaceFormatKHR swapchainFormat;
	VkSwapchainKHR swapchain;
	VkImage* swapchainImages;
	VkImageView* swapchainImageViews;
	VkRenderPass renderPass;
	VkFramebuffer* framebuffers;
	VkCommandPool renderingCommandPool;
	VkCommandBuffer renderingCommandBuffer;
	VkSemaphore imageAcquireSemaphore;
	VkSemaphore imageRenderSemaphore;
	uint32_t currentSwapchainIndex;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	VkShaderModule vertexShaderModule;
	VkShaderModule fragmentShaderModule;
	shaderc_compiler_t shaderCompiler;
	RenderingBuffer quadBuffer;
	RenderingBuffer transformationMatrixBuffer;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet transformationMatrixDescriptorSet;
	VkDescriptorSetLayout materialDescriptorSetLayout;
	VkSampler sampler;
	ImageData* images;
	size_t imageCount;
	QuadRenderCommand* quadRenderCommands;
	size_t quadRenderCommandCapacity;
	size_t quadRenderCommandCount;
	bool noSwapchain;
} Renderer;

extern VkInstance vkInstance;
extern VkSurfaceKHR vkSurface;
static Renderer renderer;

static void CreateDevice(void)
{
	uint32_t physicalDeviceCount = 0;
	VK_CHECK(vkEnumeratePhysicalDevices(vkInstance,&physicalDeviceCount,NULL));
	VkPhysicalDevice* physicalDevices = malloc(physicalDeviceCount * sizeof(*physicalDevices));
	if(!physicalDevices)
	{
		AbortApplication("Couldn't allocate %zu bytes of memory.",physicalDeviceCount * sizeof(*physicalDevices));
	}
	VK_CHECK(vkEnumeratePhysicalDevices(vkInstance,&physicalDeviceCount,physicalDevices),free(physicalDevices));

	//Discrete GPUs are usually the best in performance, so let's search for one if it exists.
	renderer.physicalDevice = physicalDevices[0];
	for(size_t i = 1;i < physicalDeviceCount;++i)
	{
		VkPhysicalDeviceProperties properties = {0};
		vkGetPhysicalDeviceProperties(physicalDevices[i],&properties);
		if(properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			renderer.physicalDevice = physicalDevices[i];
			break;
		}
	}
	free(physicalDevices);

	uint32_t queueFamilyPropertyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(renderer.physicalDevice,&queueFamilyPropertyCount,NULL);
	VkQueueFamilyProperties* queueFamilyProperties = malloc(queueFamilyPropertyCount * sizeof(*queueFamilyProperties));
	if(!queueFamilyProperties)
	{
		AbortApplication("Couldn't allocate %zu bytes of memory.",queueFamilyPropertyCount * sizeof(*queueFamilyProperties));
	}
	vkGetPhysicalDeviceQueueFamilyProperties(renderer.physicalDevice,&queueFamilyPropertyCount,queueFamilyProperties);

	renderer.graphicsQueueFamilyIndex = UINT32_MAX;
	for(uint32_t i = 0;i < queueFamilyPropertyCount;++i)
	{
		if(queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			VkBool32 surfaceSupported = VK_FALSE;
			VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(renderer.physicalDevice,i,vkSurface,&surfaceSupported));
			if(surfaceSupported)
			{
				renderer.graphicsQueueFamilyIndex = i;
				break;
			}
		}
	}
	free(queueFamilyProperties);
	if(renderer.graphicsQueueFamilyIndex == UINT32_MAX)
	{
		AbortApplication("Couldn't find quaue family that supportes both graphics rendering and current surface.");
	}

	VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = renderer.graphicsQueueFamilyIndex,
		.queueCount = 1,
		.pQueuePriorities = &(float){1.0f}
	};
	VkDeviceCreateInfo deviceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pEnabledFeatures = &(VkPhysicalDeviceFeatures){0},
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &deviceQueueCreateInfo,
		.enabledExtensionCount = 1,
		.ppEnabledExtensionNames = &(const char*){VK_KHR_SWAPCHAIN_EXTENSION_NAME}
	};
	VK_CHECK(vkCreateDevice(renderer.physicalDevice,&deviceCreateInfo,NULL,&renderer.device));
	vkGetDeviceQueue(renderer.device,renderer.graphicsQueueFamilyIndex,0,&renderer.graphicsQueue);
}

static void CreateSwapchain(void)
{
	uint32_t surfaceFormatCount = 0;
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(renderer.physicalDevice,vkSurface,&surfaceFormatCount,NULL));
	VkSurfaceFormatKHR* surfaceFormats = malloc(surfaceFormatCount * sizeof(*surfaceFormats));
	if(!surfaceFormats)
	{
		AbortApplication("Couldn't allocate %zu bytes of memory.",surfaceFormatCount * sizeof(*surfaceFormats));
	}
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(renderer.physicalDevice,vkSurface,&surfaceFormatCount,surfaceFormats),free(surfaceFormats));
	renderer.swapchainFormat = surfaceFormats[0];
	//TODO: Search for best format if it could be determined which format is considered best.
	free(surfaceFormats);

	VkSurfaceCapabilitiesKHR surfaceCapabilities = {0};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(renderer.physicalDevice,vkSurface,&surfaceCapabilities);
	renderer.swapchainImageCount = surfaceCapabilities.minImageCount + 1;
	if(renderer.swapchainImageCount > surfaceCapabilities.maxImageCount && surfaceCapabilities.maxImageCount > 0)
	{
		renderer.swapchainImageCount = surfaceCapabilities.maxImageCount;
	}

	renderer.noSwapchain = false;
	if(surfaceCapabilities.currentExtent.width == 0 || surfaceCapabilities.currentExtent.height == 0)
	{
		renderer.noSwapchain = true;
		return;
	}

	GetMainWindowSize(&renderer.swapchainImageExtent.width,&renderer.swapchainImageExtent.height);

	VkSwapchainCreateInfoKHR swapchainCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = vkSurface,
		.clipped = VK_TRUE,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.imageArrayLayers = 1,
		.imageColorSpace = renderer.swapchainFormat.colorSpace,
		.imageFormat = renderer.swapchainFormat.format,
		.imageExtent = renderer.swapchainImageExtent,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.presentMode = VK_PRESENT_MODE_FIFO_KHR,
		.preTransform = surfaceCapabilities.currentTransform,
		.minImageCount = renderer.swapchainImageCount
	};
	VK_CHECK(vkCreateSwapchainKHR(renderer.device,&swapchainCreateInfo,NULL,&renderer.swapchain));

	VK_CHECK(vkGetSwapchainImagesKHR(renderer.device,renderer.swapchain,&renderer.swapchainImageCount,NULL));
	renderer.swapchainImages = malloc(renderer.swapchainImageCount * sizeof(*renderer.swapchainImages));
	if(!renderer.swapchainImages)
	{
		AbortApplication("Couldn't allocate %zu bytes of memory.",renderer.swapchainImageCount * sizeof(*renderer.swapchainImages));
	}
	VK_CHECK(vkGetSwapchainImagesKHR(renderer.device,renderer.swapchain,&renderer.swapchainImageCount,renderer.swapchainImages));
	renderer.swapchainImageViews = calloc(renderer.swapchainImageCount,sizeof(*renderer.swapchainImageViews));
	if(!renderer.swapchainImageViews)
	{
		AbortApplication("Couldn't allocate %zu bytes of memory.",renderer.swapchainImageCount,sizeof(*renderer.swapchainImageViews));
	}
	for(uint32_t i = 0;i < renderer.swapchainImageCount;++i)
	{
		VkImageViewCreateInfo imageViewCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = renderer.swapchainImages[i],
			.format = renderer.swapchainFormat.format,
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
		VK_CHECK(vkCreateImageView(renderer.device,&imageViewCreateInfo,NULL,&renderer.swapchainImageViews[i]));
	}
}

static void CreateRenderPass(void)
{
	VkAttachmentDescription attachmentDescription = {
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.format = renderer.swapchainFormat.format,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE
	};
	VkSubpassDescription subpassDescription = {
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &(VkAttachmentReference){
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		},
		.pResolveAttachments = NULL
	};

	VkRenderPassCreateInfo renderPassCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &attachmentDescription,
		.subpassCount = 1,
		.pSubpasses = &subpassDescription
	};
	VK_CHECK(vkCreateRenderPass(renderer.device,&renderPassCreateInfo,NULL,&renderer.renderPass));
}

static void CreateFramebuffers(void)
{
	renderer.framebuffers = calloc(renderer.swapchainImageCount,sizeof(*renderer.framebuffers));
	if(!renderer.framebuffers)
	{
		AbortApplication("Couldn't allocate %zu bytes of memory.",renderer.swapchainImageCount * sizeof(*renderer.framebuffers));
	}
	for(uint32_t i = 0;i < renderer.swapchainImageCount;++i)
	{
		VkFramebufferCreateInfo framebufferCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = renderer.renderPass,
			.layers = 1,
			.width = renderer.swapchainImageExtent.width,
			.height = renderer.swapchainImageExtent.height,
			.attachmentCount = 1,
			.pAttachments = &renderer.swapchainImageViews[i]
		};
		VK_CHECK(vkCreateFramebuffer(renderer.device,&framebufferCreateInfo,NULL,&renderer.framebuffers[i]));
	}
}

static void CreateRenderingCommandPoolAndBuffer(void)
{
	VkCommandPoolCreateInfo commandPoolCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = renderer.graphicsQueueFamilyIndex
	};
	VK_CHECK(vkCreateCommandPool(renderer.device,&commandPoolCreateInfo,NULL,&renderer.renderingCommandPool));

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = renderer.renderingCommandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};
	VK_CHECK(vkAllocateCommandBuffers(renderer.device,&commandBufferAllocateInfo,&renderer.renderingCommandBuffer));
}

static void CreateSemaphores(void)
{
	VkSemaphoreCreateInfo semaphoreCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};
	VK_CHECK(vkCreateSemaphore(renderer.device,&semaphoreCreateInfo,NULL,&renderer.imageAcquireSemaphore));
	VK_CHECK(vkCreateSemaphore(renderer.device,&semaphoreCreateInfo,NULL,&renderer.imageRenderSemaphore));
}

static void RecordCommandBuffer(void)
{
	VkCommandBufferBeginInfo commandBufferBeginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	VK_CHECK(vkBeginCommandBuffer(renderer.renderingCommandBuffer,&commandBufferBeginInfo));

	VkRenderPassBeginInfo renderPassBeginInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.framebuffer = renderer.framebuffers[renderer.currentSwapchainIndex],
		.renderPass = renderer.renderPass,
		.renderArea = {
			.offset = {0,0},
			.extent = renderer.swapchainImageExtent
		},
		.clearValueCount = 1,
		.pClearValues = &(VkClearValue){
			.color = {
				.float32 = {0.0f,0.0f,0.0f,1.0f}
			}
		}
	};

	vkCmdUpdateBuffer(renderer.renderingCommandBuffer,renderer.transformationMatrixBuffer.buffer,0,sizeof(TransformationMatrix),&(TransformationMatrix){
		.matrix = Mat4Orthographic(0,(float)renderer.swapchainImageExtent.width,0,(float)renderer.swapchainImageExtent.height,-1,1)
	});
	vkCmdBeginRenderPass(renderer.renderingCommandBuffer,&renderPassBeginInfo,VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(renderer.renderingCommandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,renderer.pipeline);

	vkCmdBindVertexBuffers(renderer.renderingCommandBuffer,0,1,&renderer.quadBuffer.buffer,&(VkDeviceSize){0});

	vkCmdBindDescriptorSets(renderer.renderingCommandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,renderer.pipelineLayout,0,1,&renderer.transformationMatrixDescriptorSet,0,NULL);
	for(size_t i = 0;i < renderer.quadRenderCommandCount;++i)
	{
		QuadRenderCommand* cmd = &renderer.quadRenderCommands[i];
		ImageData* image = &renderer.images[cmd->image];
		vkCmdBindDescriptorSets(renderer.renderingCommandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,renderer.pipelineLayout,1,1,&image->descriptorSet,0,NULL);
		vkCmdPushConstants(renderer.renderingCommandBuffer,renderer.pipelineLayout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(TransformationMatrix),&(TransformationMatrix){
			.matrix = Mat4Mul(Mat4Translate(cmd->position),Mat4Scale(cmd->size))
		});
		vkCmdDraw(renderer.renderingCommandBuffer,(uint32_t)(renderer.quadBuffer.size / sizeof(Vertex)),1,0,0);
	}
	vkCmdEndRenderPass(renderer.renderingCommandBuffer);

	VK_CHECK(vkEndCommandBuffer(renderer.renderingCommandBuffer));
}

static void CreatePipelineLayout(void)
{
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 2,
		.pSetLayouts = (VkDescriptorSetLayout[]){
			renderer.descriptorSetLayout,
			renderer.materialDescriptorSetLayout
		},
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &(VkPushConstantRange){
			.offset = 0,
			.size = sizeof(TransformationMatrix),
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
		}
	};
	VK_CHECK(vkCreatePipelineLayout(renderer.device,&pipelineLayoutCreateInfo,NULL,&renderer.pipelineLayout));
}

static void CreateShaderCompiler(void)
{
	renderer.shaderCompiler = shaderc_compiler_initialize();
	if(!renderer.shaderCompiler)
	{
		AbortApplication("Couldn't create the shader compiler.");
	}
}

static void CreateShader(const char* source,size_t sourceLength,const char* name,shaderc_shader_kind shaderKind,VkShaderModule* outShaderModule)
{
	shaderc_compilation_result_t result = shaderc_compile_into_spv(renderer.shaderCompiler,source,sourceLength,shaderKind,name,"main",NULL);
	if(!result)
	{
		AbortApplication("Couldn't compile shader \"%s\".",name);
	}
	if(shaderc_result_get_num_errors(result) > 0)
	{
		char errorBuffer[2048] = {0};
		strncpy(errorBuffer,shaderc_result_get_error_message(result),sizeof(errorBuffer) - 1);
		shaderc_result_release(result);
		AbortApplication("Error during compilation of shader \"%s\": %s",name,errorBuffer);
	}

	VkShaderModuleCreateInfo shaderModuleCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = shaderc_result_get_length(result),
		.pCode = (const uint32_t*)shaderc_result_get_bytes(result)
	};
	VK_CHECK(vkCreateShaderModule(renderer.device,&shaderModuleCreateInfo,NULL,outShaderModule),shaderc_result_release(result));
	shaderc_result_release(result);
}

static void CreateShaders(void)
{
	const char vertexShaderSource[] = "#version 450\n"
	"layout(location = 0) in vec2 position;"
	"layout(location = 1) in vec2 textureCoords;"
	"layout(location = 0) out vec2 passTextureCoords;"
	"layout(set = 0,binding = 0) uniform TransformationMatrix"
	"{"
	"mat4 matrix;"
	"};"
	"layout(push_constant) uniform ObjectTransformationMatrix"
	"{"
	"mat4 objectMatrix;"
	"};"
	"void main(void)"
	"{"
	"gl_Position = matrix * objectMatrix * vec4(position,0.0,1.0);"
	"passTextureCoords = textureCoords;"
	"}";
	CreateShader(vertexShaderSource,sizeof(vertexShaderSource) - 1,"vertex shader",shaderc_vertex_shader,&renderer.vertexShaderModule);

	const char fragmentShaderModule[] = "#version 450\n"
	"layout(location = 0) in vec2 passTextureCoords;"
	"layout(location = 0) out vec4 outColor;"
	"layout(set = 1,binding = 0) uniform sampler2D texture0;"
	"void main(void)"
	"{"
	"outColor = texture(texture0,passTextureCoords);"
	"}";
	CreateShader(fragmentShaderModule,sizeof(fragmentShaderModule) - 1,"fragment shader",shaderc_fragment_shader,&renderer.fragmentShaderModule);
}

static void CreatePipeline(void)
{
	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &(VkVertexInputBindingDescription){
			.binding = 0,
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
			.stride = sizeof(Vertex)
		},
		.vertexAttributeDescriptionCount = 2,
		.pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[]){
			{
				.binding = 0,
				.format = VK_FORMAT_R32G32_SFLOAT,
				.location = 0,
				.offset = offsetof(Vertex,position)
			},
			{
				.binding = 0,
				.format = VK_FORMAT_R32G32_SFLOAT,
				.location = 1,
				.offset = offsetof(Vertex,textureCoords)
			}
		}
	};
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.lineWidth = 1.0f
	};
	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &(VkPipelineColorBlendAttachmentState){
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
			.blendEnable = VK_TRUE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.alphaBlendOp = VK_BLEND_OP_SUBTRACT
		},
		.blendConstants = {1.0f,1.0f,1.0f,1.0f}
	};
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
	};
	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};
	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &(VkViewport){
			.width = (float)renderer.swapchainImageExtent.width,
			.height = (float)renderer.swapchainImageExtent.height,
			.maxDepth = 1.0f
		},
		.scissorCount = 1,
		.pScissors = &(VkRect2D){
			.offset = {0,0},
			.extent = renderer.swapchainImageExtent
		}
	};
	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.module = renderer.vertexShaderModule,
			.pName = "main",
			.stage = VK_SHADER_STAGE_VERTEX_BIT
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.module = renderer.fragmentShaderModule,
			.pName = "main",
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT
		}
	};

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.basePipelineIndex = -1,
		.layout = renderer.pipelineLayout,
		.renderPass = renderer.renderPass,
		.pVertexInputState = &vertexInputStateCreateInfo,
		.pInputAssemblyState = &inputAssemblyStateCreateInfo,
		.pRasterizationState = &rasterizationStateCreateInfo,
		.pColorBlendState = &colorBlendStateCreateInfo,
		.pDepthStencilState = &depthStencilStateCreateInfo,
		.pMultisampleState = &multisampleStateCreateInfo,
		.pViewportState = &viewportStateCreateInfo,
		.stageCount = sizeof(shaderStageCreateInfos) / sizeof(*shaderStageCreateInfos),
		.pStages = shaderStageCreateInfos
	};
	VK_CHECK(vkCreateGraphicsPipelines(renderer.device,VK_NULL_HANDLE,1,&pipelineCreateInfo,NULL,&renderer.pipeline));
}

static void CreateDescriptorSetLayouts(void)
{
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &(VkDescriptorSetLayoutBinding){
			.binding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
		}
	};
	VK_CHECK(vkCreateDescriptorSetLayout(renderer.device,&descriptorSetLayoutCreateInfo,NULL,&renderer.descriptorSetLayout));

	VkDescriptorSetLayoutCreateInfo materialDescriptorSetLayoutCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &(VkDescriptorSetLayoutBinding){
			.binding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
		}
	};
	VK_CHECK(vkCreateDescriptorSetLayout(renderer.device,&materialDescriptorSetLayoutCreateInfo,NULL,&renderer.materialDescriptorSetLayout));
}

static void CreateDescriptorPool(void)
{
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets = 16,
		.poolSizeCount = 2,
		.pPoolSizes = (VkDescriptorPoolSize[]){
			{
				.descriptorCount = 1,
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
			},
			{
				.descriptorCount = 16,
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
			}
		}
	};
	VK_CHECK(vkCreateDescriptorPool(renderer.device,&descriptorPoolCreateInfo,NULL,&renderer.descriptorPool));
}

static void CreateSampler(void)
{
	VkSamplerCreateInfo samplerCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.minFilter = VK_FILTER_NEAREST,
		.magFilter = VK_FILTER_NEAREST,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT
	};
	VK_CHECK(vkCreateSampler(renderer.device,&samplerCreateInfo,NULL,&renderer.sampler));
}

static void CreateSwapchainRelatives(void)
{
	CreateSwapchain();
	if(!renderer.noSwapchain)
	{
		CreateRenderPass();
		CreatePipeline();
		CreateFramebuffers();
	}
}

static void DestroySwapchainRelatives(void)
{
	if(renderer.graphicsQueue)
	{
		vkQueueWaitIdle(renderer.graphicsQueue);
	}
	for(uint32_t i = 0;i < renderer.swapchainImageCount;++i)
	{
		vkDestroyFramebuffer(renderer.device,renderer.framebuffers[i],NULL);
	}
	free(renderer.framebuffers);
	renderer.framebuffers = NULL;
	vkDestroyPipeline(renderer.device,renderer.pipeline,NULL);
	renderer.pipeline = VK_NULL_HANDLE;
	vkDestroyRenderPass(renderer.device,renderer.renderPass,NULL);
	renderer.renderPass = VK_NULL_HANDLE;
	for(uint32_t i = 0;i < renderer.swapchainImageCount;++i)
	{
		vkDestroyImageView(renderer.device,renderer.swapchainImageViews[i],NULL);
	}
	renderer.swapchainImageCount = 0;
	free(renderer.swapchainImageViews);
	renderer.swapchainImageViews = NULL;
	free(renderer.swapchainImages);
	renderer.swapchainImages = NULL;
	vkDestroySwapchainKHR(renderer.device,renderer.swapchain,NULL);
	renderer.swapchain = VK_NULL_HANDLE;
}

void InitRenderer(void)
{
	CreateShaderCompiler();
	CreateDevice();
	CreateSampler();
	CreateDescriptorSetLayouts();
	CreateDescriptorPool();
	CreateShaders();
	CreatePipelineLayout();
	CreateSemaphores();
	CreateRenderingCommandPoolAndBuffer();
	CreateSwapchainRelatives();

	Vertex quadVertices[] = {
		{{0,0},{0,0}},
		{{1,0},{1,0}},
		{{1,1},{1,1}},
		{{0,0},{0,0}},
		{{1,1},{1,1}},
		{{0,1},{0,1}},
	};
	if(!CreateRenderingBuffer(renderer.device,renderer.physicalDevice,sizeof(quadVertices),quadVertices,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,&renderer.quadBuffer))
	{
		AbortApplication(GetError());
	}
	if(!CreateRenderingBuffer(renderer.device,renderer.physicalDevice,sizeof(TransformationMatrix),NULL,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,&renderer.transformationMatrixBuffer))
	{
		AbortApplication(GetError());
	}

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = renderer.descriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &renderer.descriptorSetLayout
	};
	VK_CHECK(vkAllocateDescriptorSets(renderer.device,&descriptorSetAllocateInfo,&renderer.transformationMatrixDescriptorSet));

	VkWriteDescriptorSet writeDescriptorSet = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.dstArrayElement = 0,
		.dstBinding = 0,
		.dstSet = renderer.transformationMatrixDescriptorSet,
		.pBufferInfo = &(VkDescriptorBufferInfo){
			.buffer = renderer.transformationMatrixBuffer.buffer,
			.offset = 0,
			.range = renderer.transformationMatrixBuffer.size
		},
		.pImageInfo = NULL,
		.pTexelBufferView = NULL
	};
	vkUpdateDescriptorSets(renderer.device,1,&writeDescriptorSet,0,NULL);

	renderer.quadRenderCommandCapacity = 256;
	renderer.quadRenderCommands = malloc(renderer.quadRenderCommandCapacity * sizeof(*renderer.quadRenderCommands));
	if(!renderer.quadRenderCommands)
	{
		AbortApplication("Couldn't allocate %zu bytes of memory.",renderer.quadRenderCommandCapacity * sizeof(*renderer.quadRenderCommands));
	}
}

void TermRenderer(void)
{
	if(renderer.device)
	{
		if(renderer.graphicsQueue)
		{
			vkQueueWaitIdle(renderer.graphicsQueue);
		}
		free(renderer.quadRenderCommands);
		for(size_t i = 0;i < renderer.imageCount;++i)
		{
			vkFreeDescriptorSets(renderer.device,renderer.descriptorPool,1,&renderer.images[i].descriptorSet);
			DestroyRenderingImage(renderer.device,renderer.images[i].image);
		}

		DestroyRenderingBuffer(renderer.device,renderer.transformationMatrixBuffer);
		DestroyRenderingBuffer(renderer.device,renderer.quadBuffer);
		DestroySwapchainRelatives();
		vkDestroyCommandPool(renderer.device,renderer.renderingCommandPool,NULL);
		vkDestroySemaphore(renderer.device,renderer.imageRenderSemaphore,NULL);
		vkDestroySemaphore(renderer.device,renderer.imageAcquireSemaphore,NULL);
		vkDestroyPipelineLayout(renderer.device,renderer.pipelineLayout,NULL);
		vkDestroyShaderModule(renderer.device,renderer.fragmentShaderModule,NULL);
		vkDestroyShaderModule(renderer.device,renderer.vertexShaderModule,NULL);
		vkDestroyDescriptorPool(renderer.device,renderer.descriptorPool,NULL);
		vkDestroyDescriptorSetLayout(renderer.device,renderer.materialDescriptorSetLayout,NULL);
		vkDestroyDescriptorSetLayout(renderer.device,renderer.descriptorSetLayout,NULL);
		vkDestroySampler(renderer.device,renderer.sampler,NULL);
	}
	vkDestroyDevice(renderer.device,NULL);
	shaderc_compiler_release(renderer.shaderCompiler);
}

void BeginRendering(void)
{
	renderer.quadRenderCommandCount = 0;
}

void EndRendering(void)
{
	if(renderer.noSwapchain)
	{
		if(!IsMainWindowMinimized())
		{
			CreateSwapchainRelatives();
		}
		return;
	}

	VkResult result = vkAcquireNextImageKHR(renderer.device,renderer.swapchain,UINT64_MAX,renderer.imageAcquireSemaphore,VK_NULL_HANDLE,&renderer.currentSwapchainIndex);
	if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		DestroySwapchainRelatives();
		CreateSwapchainRelatives();
		return;
	}
	else if(result != VK_SUCCESS)
	{
		AbortApplication("Function vkAcquireNextImageKHR returned %s.",VkResultToString(result));
	}
	RecordCommandBuffer();

	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &renderer.renderingCommandBuffer,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &renderer.imageAcquireSemaphore,
		.pWaitDstStageMask = &(VkPipelineStageFlags){VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &renderer.imageRenderSemaphore
	};
	VK_CHECK(vkQueueSubmit(renderer.graphicsQueue,1,&submitInfo,VK_NULL_HANDLE));

	VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.swapchainCount = 1,
		.pSwapchains = &renderer.swapchain,
		.pImageIndices = &renderer.currentSwapchainIndex,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &renderer.imageRenderSemaphore
	};

	result = vkQueuePresentKHR(renderer.graphicsQueue,&presentInfo);
	if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		DestroySwapchainRelatives();
		CreateSwapchainRelatives();
		return;
	}
	else if(result != VK_SUCCESS)
	{
		AbortApplication("Function vkAcquireNextImageKHR returned %s.",VkResultToString(result));
	}
	VK_CHECK(vkQueueWaitIdle(renderer.graphicsQueue));
}

bool LoadTexture(const char* filePath,Image* outImage)
{
	SDL_Surface* surface = IMG_Load(filePath);
	if(!surface)
	{
		SetError(IMG_GetError());
		return false;
	}
	SDL_Surface* convertedSurface = SDL_ConvertSurfaceFormat(surface,SDL_PIXELFORMAT_RGBA32,0);
	SDL_FreeSurface(surface);
	if(!convertedSurface)
	{
		SetError(IMG_GetError());
		return false;
	}

	ImageData newImageData = {0};
	if(!CreateRenderingImage(renderer.device,renderer.physicalDevice,(VkExtent2D){convertedSurface->w,convertedSurface->h},VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,& newImageData.image))
	{
		SDL_FreeSurface(convertedSurface);
		return false;
	}
	if(!FillImageTexels(renderer.device,renderer.physicalDevice,renderer.renderingCommandPool,renderer.graphicsQueue,convertedSurface->pixels,newImageData.image))
	{
		DestroyRenderingImage(renderer.device,newImageData.image);
		SDL_FreeSurface(convertedSurface);
		return false;
	}
	SDL_FreeSurface(convertedSurface);

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = renderer.descriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &renderer.materialDescriptorSetLayout
	};
	VkResult result = vkAllocateDescriptorSets(renderer.device,&descriptorSetAllocateInfo,&newImageData.descriptorSet);
	if(result != VK_SUCCESS)
	{
		DestroyRenderingImage(renderer.device,newImageData.image);
		return false;
	}

	VkWriteDescriptorSet writeDescriptorSet = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.dstArrayElement = 0,
		.dstBinding = 0,
		.dstSet = newImageData.descriptorSet,
		.pImageInfo = &(VkDescriptorImageInfo){
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.imageView = newImageData.image.imageView,
			.sampler = renderer.sampler
		},
		.pBufferInfo = NULL,
		.pTexelBufferView = NULL
	};
	vkUpdateDescriptorSets(renderer.device,1,&writeDescriptorSet,0,NULL);

	ImageData* tmp = realloc(renderer.images,(renderer.imageCount + 1) * sizeof(*tmp));
	if(!tmp)
	{
		vkFreeDescriptorSets(renderer.device,renderer.descriptorPool,1,&newImageData.descriptorSet);
		DestroyRenderingImage(renderer.device,newImageData.image);
		SetError("Couldn't allocate %zu bytes of memory.",sizeof(*tmp));
		return false;
	}
	renderer.images = tmp;
	++renderer.imageCount;
	renderer.images[renderer.imageCount - 1] = newImageData;
	if(outImage)
	{
		*outImage = renderer.imageCount - 1;
	}
	return true;
}

bool RenderQuad(const QuadRenderCommand* cmd)
{
	if((renderer.quadRenderCommandCount + 1) > renderer.quadRenderCommandCapacity)
	{
		QuadRenderCommand* tmp = realloc(renderer.quadRenderCommands,(renderer.quadRenderCommandCapacity + 256) * sizeof(*tmp));
		if(!tmp)
		{
			SetError("Couldn't allocate %zu bytes of memory.",256 * sizeof(*tmp));
			return false;
		}
		renderer.quadRenderCommands = tmp;
		renderer.quadRenderCommandCapacity += 256;
	}
	++renderer.quadRenderCommandCount;
	renderer.quadRenderCommands[renderer.quadRenderCommandCount - 1] = *cmd;
	return true;
}