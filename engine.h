#ifndef ENGINE_H
#define ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL_scancode.h>

void InitEngine(void);
void TermEngine(void);
void CreateMainWindow(const char* title,int width,int height);
void ProcessEvents(void);
void GetMainWindowSize(uint32_t* width,uint32_t* height);
bool IsMainWindowMinimized(void);
float GetDeltaTime(void);
bool IsKeyPressed(SDL_Scancode key);
bool WasKeyPressed(SDL_Scancode key);
void ResetTimer(void);

#endif