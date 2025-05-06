#include "SDL.h"
#include "SDL_ttf.h"
#include "SDL_image.h"
#include <assert.h>
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

typedef SDL_Point v2;
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

#define UI_MAX_DRAW_CMD 1024
#define UI_MAX_STATE 1024

// UI Draw Command

typedef enum {
  // Rectangle
  UI_RECT,
  // Texture/image
  UI_IMAGE,
} UI_DrawCmdType;

typedef struct {
  UI_DrawCmdType type;
  Rect rect;
  void* image;
} UI_DrawCmd;

// UI Draw Queue

UI_DrawCmd ui_draw_queue[UI_MAX_DRAW_CMD] = {};
i32 ui_draw_queue_length = 0;

UI_DrawCmd *UI_DrawQueuePush() {
  assert(ui_draw_queue_length < UI_MAX_DRAW_CMD);

  return &ui_draw_queue[ui_draw_queue_length++];
}

// UI State

typedef enum {
  UI_LAYOUT_HORIZONTAL,
  UI_LAYOUT_VERTICAL,
} UI_Layout;

typedef struct {
  v2 pos;
  UI_Layout layout;
} UI_State;

const UI_State ui_default_state = {
  .pos = {0, 0},
};

UI_State ui_state_stack[1024] = { ui_default_state };
i32 ui_state_stack_length = 0;
UI_State *ui = ui_state_stack;

void UI_Clear() {
  ui_draw_queue_length = 0;
  ui_state_stack_length = 0;
  ui = &ui_state_stack[ui_state_stack_length];
  *ui = ui_default_state;
}

void UI_PushState() {
  assert(ui_state_stack_length < UI_MAX_STATE);

  ui[1] = ui[0];
  ui_state_stack_length++;
  ui++;
}

void UI_PopState() {
  assert(ui_state_stack_length > 0);

  ui_state_stack_length--;
  ui--;
}

// UI Layout

void UI_UpdateLayout() {
  UI_DrawCmd *last_cmd = &ui_draw_queue[ui_draw_queue_length - 1];
  switch (ui->layout) {
    case UI_LAYOUT_HORIZONTAL:
      ui->pos.x += last_cmd->rect.w;
      break;
    case UI_LAYOUT_VERTICAL:
      ui->pos.y += last_cmd->rect.h;
      break;
      break;
  }
}

// UI Widgets

// UI Rect

void UI_Rect(i32 w, i32 h) {
  UI_DrawCmd *cmd = UI_DrawQueuePush();
  cmd->type = UI_RECT;
  cmd->rect = (Rect){ui->pos.x, ui->pos.y, w, h};
  UI_UpdateLayout();
}

// UI Panel

void UI_BeginPanel() {
  
}

// END UI library

// UI Renderer

void UI_Render() {
  for (i32 i = 0; i < ui_draw_queue_length; i++) {
    UI_DrawCmd *cmd = &ui_draw_queue[i];
    switch (cmd->type) {
      case UI_RECT:
        SDL_RenderDrawRect(renderer, (Rect*)&cmd->rect);
        break;
      case UI_IMAGE:
        SDL_RenderCopy(renderer, cmd->image, NULL, (Rect*)&cmd->rect);
        break;
    }
  }
}

// END UI Renderer

i32 main() {
  InitSDL();

  SDL_Event event;

  // ui_draw_queue[0] = (UI_DrawCmd){
  //   .prim = UI_RECT,
  //   .rect = { 10, 10, 100, 50 },
  // };
  // ui_draw_queue[1] = (UI_DrawCmd){
  //   .prim = UI_RECT,
  //   .rect = { 15, 15, 100, 50 },
  // };
  // ui_draw_queue_length = 2;

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
        UI_Clear();

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        
        ui->layout = UI_LAYOUT_VERTICAL;
        UI_Rect(100, 50);
        UI_Rect(100, 50);
        UI_Rect(100, 50);
        ui->layout = UI_LAYOUT_HORIZONTAL;
        UI_Rect(200, 50);
        UI_Rect(200, 50);
        UI_Rect(200, 50);
        
        UI_Render();
      }

      SDL_RenderPresent(renderer);
    }

    // Throttle FPS.
    SDL_Delay(1000/16);
  }

  return EXIT_SUCCESS;
}
