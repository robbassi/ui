#include "SDL.h"
#include "SDL_ttf.h"
#include "SDL_image.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define FONT "fixedsys.ttf"

SDL_Window *window;
SDL_Renderer *renderer;
TTF_Font *font;

void HandleSDLError(const char *context) {
  printf("[%s]: %s\n", context, SDL_GetError());
  exit(EXIT_FAILURE);
}

void InitSDL() {
  if (SDL_Init(SDL_INIT_EVERYTHING ^ SDL_INIT_AUDIO) < 0) {
    HandleSDLError("SDL_Init");
  }

  window = SDL_CreateWindow("SDL Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
  if (!window) {
    HandleSDLError("SDL_CreateWindow");
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    HandleSDLError("SDL_CreateRenderer");
  }

  if (TTF_Init() < 0) {
    HandleSDLError("TTF_Init");
  }

  font = TTF_OpenFont(FONT, 16);
}

typedef SDL_Point Point;
typedef SDL_Rect Rect;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;

// UI library

#define UI_MAX_STATE 1024
#define UI_MAX_DRAW_CMD 1024

// UI State

typedef struct {
  i32 x;
} UI_State;

UI_State ui_state_stack[1024] = {
  { // default state
  }
};

UI_State *ui = &ui_state_stack[0];

// UI Primitive

typedef enum {
  // Rectangle
  UI_RECT,
  // Texture/image
  UI_IMAGE,
} UI_Primitive;

typedef struct {
  i32 x, y, w, h;
} UI_RectData;

// UI Draw Command

typedef struct {
  UI_Primitive prim;
  UI_RectData rect;
  void* image;
} UI_DrawCmd;

// UI Draw Queue

UI_DrawCmd ui_draw_queue[UI_MAX_DRAW_CMD] = {};
i32 ui_draw_queue_length = 0;

// END UI library

// UI Renderer

void UI_Render() {
  for (i32 i = 0; i < ui_draw_queue_length; i++) {
    UI_DrawCmd *draw_cmd = &ui_draw_queue[i];
    switch (draw_cmd->prim) {
      case UI_RECT:
        SDL_RenderDrawRect(renderer, (Rect*)&draw_cmd->rect);
        break;
      case UI_IMAGE:
        SDL_RenderCopy(renderer, draw_cmd->image, NULL, (Rect*)&draw_cmd->rect);
        break;
    }
  }
}

// END UI Renderer

i32 main() {
  InitSDL();

  SDL_Event event;

  ui_draw_queue[0] = (UI_DrawCmd){
    .prim = UI_RECT,
    .rect = { 10, 10, 100, 50 },
  };
  ui_draw_queue[1] = (UI_DrawCmd){
    .prim = UI_RECT,
    .rect = { 15, 15, 100, 50 },
  };
  ui_draw_queue_length = 2;

  while (true) {

    // Handle events.
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT:
          exit(1);
      }
    }

    // Render.
    {
      SDL_SetRenderDrawColor(renderer, 60, 80, 40, 255);
      SDL_RenderClear(renderer);

      // Draw UI
      {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        UI_Render();
      }

      SDL_RenderPresent(renderer);
    }

    // Throttle FPS.
    SDL_Delay(1000/16);
  }

  return EXIT_SUCCESS;
}
