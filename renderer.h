#ifndef RENDERER_H
#define RENDERER_H

#include <stddef.h>
#include <stdbool.h>
#include "math.h"

typedef struct Vertex
{
	Vec2 position;
	Vec2 textureCoords;
} Vertex;

typedef size_t Image;
typedef struct QuadRenderCommand
{
	Vec2 position;
	Vec2 size;
	Image image;
} QuadRenderCommand;

void InitRenderer(void);
void TermRenderer(void);
void BeginRendering(void);
void EndRendering(void);
bool LoadTexture(const char* filePath,Image* outImage);
bool RenderQuad(const QuadRenderCommand* cmd);

#endif