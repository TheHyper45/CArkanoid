#include "quit.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <SDL_messagebox.h>
#include "main.h"
#include "engine.h"
#include "renderer.h"

static char errorBuffer[4096];

void SetError(const char* fmt,...)
{
	va_list list;
	va_start(list,fmt);
	vsnprintf(errorBuffer,sizeof(errorBuffer) - 1,fmt,list);
	va_end(list);
}

const char* GetError(void)
{
	return errorBuffer;
}

static void CleanUp(void)
{
	TermGame();
	TermRenderer();
	TermEngine();
}

_Noreturn void ExitApplication(void)
{
	CleanUp();
	exit(EXIT_SUCCESS);
}

_Noreturn void AbortApplication(const char* fmt,...)
{
	CleanUp();

	va_list list;
	va_start(list,fmt);
	vsnprintf(errorBuffer,sizeof(errorBuffer) - 1,fmt,list);
	va_end(list);
	if(SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Error!",errorBuffer,NULL) != 0)
	{
		fprintf(stderr,"Error: %s",errorBuffer);
		fflush(stderr);
	}

	exit(EXIT_FAILURE);
}