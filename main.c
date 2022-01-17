#include <SDL_main.h>

#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include "quit.h"
#include "engine.h"
#include "renderer.h"

#define PLAYER_WIDTH 128
#define PLAYER_HEIGHT 16
#define BALL_WIDTH 16
#define BALL_HEIGHT 16
#define BRICK_WIDTH 128
#define BRICK_HEIGHT 32
#define BRICK_GRID_WIDTH 8
#define BRICK_GRID_HEIGHT 5

typedef struct Brick
{
	QuadRenderCommand cmd;
	bool destroyed;
} Brick;

static Brick* bricks;
static size_t brickCapacity;
static size_t brickCount;
static QuadRenderCommand ballCmd;
static Vec2 ballVelocity;
static QuadRenderCommand playerCmd;
static Image titleImage = 0;
static Image loseScreenImage = 0;
static Image winScreenImage = 0;
static Image ballImage = 0;
static Image playerImage = 0;
static Image brickImages[4] = {0};

void TermGame(void)
{
	free(bricks);
}

static bool AreColliding(const QuadRenderCommand* a,const QuadRenderCommand* b)
{
	return (a->position.x + a->size.x) >= b->position.x && a->position.x <= (b->position.x + b->size.x) && (a->position.y + a->size.y) >= b->position.y && a->position.y <= (b->position.y + b->size.y);
}

static void InitGame(void)
{
	ballCmd = (QuadRenderCommand){
		.position = {512 - BALL_WIDTH / 2,386 - BALL_HEIGHT / 2},
		.size = {BALL_WIDTH,BALL_HEIGHT},
		.image = ballImage
	};
	ballVelocity = (Vec2){0,200};

	playerCmd = (QuadRenderCommand){
		.position = {512 - PLAYER_WIDTH / 2,768 - 48},
		.size = {PLAYER_WIDTH,PLAYER_HEIGHT},
		.image = playerImage
	};

	free(bricks);
	brickCapacity = BRICK_GRID_WIDTH * BRICK_GRID_HEIGHT;
	bricks = malloc(brickCapacity * sizeof(*bricks));
	if(!bricks)
	{
		AbortApplication("Coudln't allocate %zu bytes of memory.",brickCapacity * sizeof(*bricks));
	}

	brickCount = 0;
	for(size_t y = 0;y < BRICK_GRID_HEIGHT;++y)
	{
		for(size_t x = 0;x < BRICK_GRID_WIDTH;++x)
		{
			bricks[y * BRICK_GRID_WIDTH + x] = (Brick){
				.cmd = {
					.size = {BRICK_WIDTH,BRICK_HEIGHT},
					.position = {(float)x * BRICK_WIDTH,(float)y * BRICK_HEIGHT},
					.image = brickImages[rand() % 4]
				}
			};
			++brickCount;
		}
	}
}

int main(int argc,char** argv)
{
	(void)argc;
	(void)argv;

	srand((unsigned)time(NULL));
	InitEngine();
	CreateMainWindow("CArkanoid",1024,768);
	InitRenderer();
	
	if(!LoadTexture("assets/title.png",&titleImage))
	{
		AbortApplication("%s",GetError());
	}
	if(!LoadTexture("assets/lose.png",&loseScreenImage))
	{
		AbortApplication("%s",GetError());
	}
	if(!LoadTexture("assets/win.png",&winScreenImage))
	{
		AbortApplication("%s",GetError());
	}
	if(!LoadTexture("assets/ball.png",&ballImage))
	{
		AbortApplication("%s",GetError());
	}
	if(!LoadTexture("assets/player.png",&playerImage))
	{
		AbortApplication("%s",GetError());
	}
	if(!LoadTexture("assets/brick0.png",&brickImages[0]))
	{
		AbortApplication("%s",GetError());
	}
	if(!LoadTexture("assets/brick1.png",&brickImages[1]))
	{
		AbortApplication("%s",GetError());
	}
	if(!LoadTexture("assets/brick2.png",&brickImages[2]))
	{
		AbortApplication("%s",GetError());
	}
	if(!LoadTexture("assets/brick3.png",&brickImages[3]))
	{
		AbortApplication("%s",GetError());
	}

	typedef enum GameState
	{
		GAME_STATE_MAIN_MENU,
		GAME_STATE_PLAY,
		GAME_STATE_WON,
		GAME_STATE_LOST,
	} GameState;
	GameState gameState = GAME_STATE_MAIN_MENU;
	
	ResetTimer();
	while(true)
	{
		ProcessEvents();
		float deltaTime = GetDeltaTime();
		if(WasKeyPressed(SDL_SCANCODE_ESCAPE))
		{
			ExitApplication();
		}

		if(gameState == GAME_STATE_PLAY)
		{
			if(IsKeyPressed(SDL_SCANCODE_R))
			{
				InitGame();
			}

			Vec2 oldPlayerPosition = playerCmd.position;
			if(IsKeyPressed(SDL_SCANCODE_RIGHT))
			{
				playerCmd.position.x += 300 * deltaTime;
				if((playerCmd.position.x + playerCmd.size.x) >= 1024)
				{
					playerCmd.position.x = 1024 - playerCmd.size.x;
				}
			}
			if(IsKeyPressed(SDL_SCANCODE_LEFT))
			{
				playerCmd.position.x -= 300 * deltaTime;
				if(playerCmd.position.x < 0)
				{
					playerCmd.position.x = 0;
				}
			}

			ballCmd.position.x += ballVelocity.x * deltaTime;
			ballCmd.position.y += ballVelocity.y * deltaTime;
			if((ballCmd.position.x + ballCmd.size.x) >= 1024)
			{
				ballCmd.position.x = 1024 - ballCmd.size.x;
				ballVelocity.x *= -1;
			}
			if(ballCmd.position.x < 0)
			{
				ballCmd.position.x = 0;
				ballVelocity.x *= -1;
			}
			if(ballCmd.position.y < 0)
			{
				ballCmd.position.y = 0;
				ballVelocity.y *= -1;
			}
			if(ballCmd.position.y >= 768)
			{
				gameState = GAME_STATE_LOST;
			}

			if(AreColliding(&playerCmd,&ballCmd))
			{
				ballCmd.position.y = playerCmd.position.y - ballCmd.size.y;
				ballVelocity.y *= -1;
				ballVelocity.x += (playerCmd.position.x - oldPlayerPosition.x) * 50;
			}

			bool noBricksLeft = true;
			for(size_t i = 0;i < brickCount;++i)
			{
				Brick* brick = &bricks[i];
				if(!brick->destroyed)
				{
					noBricksLeft = false;
					if(AreColliding(&brick->cmd,&ballCmd))
					{
						if(ballVelocity.x >= ballVelocity.y)
						{
							ballVelocity.x *= -1;
						}
						else
						{
							ballVelocity.y *= -1;
						}
						brick->destroyed = true;
					}
				}
			}
			if(noBricksLeft)
			{
				gameState = GAME_STATE_WON;
			}
		}
		else
		{
			if(IsKeyPressed(SDL_SCANCODE_RETURN))
			{
				InitGame();
				gameState = GAME_STATE_PLAY;
			}
		}

		BeginRendering();
		if(gameState == GAME_STATE_MAIN_MENU)
		{
			RenderQuad(&(QuadRenderCommand){
				.position = {0,0},
				.size = {1024,768},
				.image = titleImage
			});
		}
		else
		{
			RenderQuad(&ballCmd);
			RenderQuad(&playerCmd);
			for(size_t i = 0;i < brickCount;++i)
			{
				if(!bricks[i].destroyed)
				{
					RenderQuad(&bricks[i].cmd);
				}
			}
			if(gameState == GAME_STATE_LOST)
			{
				RenderQuad(&(QuadRenderCommand){
					.position = {0,0},
					.size = {1024,768},
					.image = loseScreenImage
				});
			}
			else if(gameState == GAME_STATE_WON)
			{
				RenderQuad(&(QuadRenderCommand){
					.position = {0,0},
					.size = {1024,768},
					.image = winScreenImage
				});
			}
		}
		EndRendering();
	}
	return 0;
}