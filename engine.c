#include "engine.h"

#include <memory.h>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_vulkan.h>
#include "quit.h"
#include "vulkan.h"

static SDL_Window* mainWindow;
static float deltaTime;
static uint64_t lastTimerValue;
static bool scancodes[SDL_NUM_SCANCODES];
static bool scancodesOnce[SDL_NUM_SCANCODES];
VkInstance vkInstance;
VkSurfaceKHR vkSurface;

static void InitVulkanAPI(void)
{
	unsigned extensionNameCount = 0;
	if(!SDL_Vulkan_GetInstanceExtensions(mainWindow,&extensionNameCount,NULL))
	{
		AbortApplication("%s",SDL_GetError());
	}

	/*
		Microsoft's compiler complains when extensionNames has type const char**.
		On ther other hand, GCC (and probalby clang too) complains that this variable is char**.
		So this weird #if is made to satisfy both compilers.
	*/
#if defined(_MSC_VER)
	char** extensionNames = malloc(extensionNameCount * sizeof(*extensionNames));
#else
	const char** extensionNames = malloc(extensionNameCount * sizeof(*extensionNames));
#endif
	if(!extensionNames)
	{
		AbortApplication("Couldn't allocate %zu bytes of memory.",extensionNameCount * sizeof(*extensionNames));
	}
	if(!SDL_Vulkan_GetInstanceExtensions(mainWindow,&extensionNameCount,extensionNames))
	{
		free(extensionNames);
		AbortApplication("%s",SDL_GetError());
	}

	VkApplicationInfo applicationInfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.apiVersion = VK_API_VERSION_1_2,
		.pApplicationName = "CArkanoid",
		.pEngineName = "CustomEngine"
	};
	VkInstanceCreateInfo instanceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &applicationInfo,
		.enabledExtensionCount = extensionNameCount,
		.ppEnabledExtensionNames = extensionNames
	};
#ifdef DEBUG_BUILD
	const char* debugLayerName = "VK_LAYER_KHRONOS_validation";
	instanceCreateInfo.enabledLayerCount = 1;
	instanceCreateInfo.ppEnabledLayerNames = &debugLayerName;
#endif

	VkResult result = vkCreateInstance(&instanceCreateInfo,NULL,&vkInstance);
	free(extensionNames);
	if(result != VK_SUCCESS)
	{
		AbortApplication("Function vkCreateInstance returned %s.",VkResultToString(result));
	}
}

void InitEngine(void)
{
	if(SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		AbortApplication("%s",SDL_GetError());
	}
	if((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != IMG_INIT_PNG)
	{
		AbortApplication("%s",IMG_GetError());
	}
}

void TermEngine(void)
{
	if(vkInstance)
	{
		vkDestroySurfaceKHR(vkInstance,vkSurface,NULL);
	}
	vkDestroyInstance(vkInstance,NULL);
	if(mainWindow)
	{
		SDL_DestroyWindow(mainWindow);
	}
	IMG_Quit();
	SDL_Quit();
}

void CreateMainWindow(const char* title,int width,int height)
{
	mainWindow = SDL_CreateWindow(title,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,width,height,SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
	if(!mainWindow)
	{
		AbortApplication("%s",SDL_GetError());
	}
	InitVulkanAPI();
	if(!SDL_Vulkan_CreateSurface(mainWindow,vkInstance,&vkSurface))
	{
		AbortApplication("%s",SDL_GetError());
	}
}

void ProcessEvents(void)
{
	uint64_t currentTimerValue = SDL_GetPerformanceCounter();
	memset(scancodesOnce,0,sizeof(scancodesOnce));
	SDL_Event event = {0};
	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
			case SDL_QUIT:
				ExitApplication();
			case SDL_KEYDOWN:
				scancodes[event.key.keysym.scancode] = true;
				if(event.key.repeat == 0)
				{
					scancodesOnce[event.key.keysym.scancode] = true;
				}
				break;
			case SDL_KEYUP:
				scancodes[event.key.keysym.scancode] = false;
				break;
			case SDL_WINDOWEVENT:
				if(event.window.event == SDL_WINDOWEVENT_RESIZED || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
				{
					currentTimerValue = SDL_GetPerformanceCounter();
				}
				break;
		}
	}
	deltaTime = (float)(currentTimerValue - lastTimerValue) / SDL_GetPerformanceFrequency();
	lastTimerValue = currentTimerValue;
}

void GetMainWindowSize(uint32_t* width,uint32_t* height)
{
	int iwidth = 0;
	int iheight = 0;
	SDL_Vulkan_GetDrawableSize(mainWindow,&iwidth,&iheight);
	*width = (uint32_t)iwidth;
	*height = (uint32_t)iheight;
}

bool IsMainWindowMinimized(void)
{
	return SDL_GetWindowFlags(mainWindow) & SDL_WINDOW_MINIMIZED;
}

float GetDeltaTime(void)
{
	return deltaTime;
}

bool IsKeyPressed(SDL_Scancode key)
{
	return scancodes[key];
}

bool WasKeyPressed(SDL_Scancode key)
{
	return scancodesOnce[key];
}

void ResetTimer(void)
{
	lastTimerValue = SDL_GetPerformanceCounter();
}