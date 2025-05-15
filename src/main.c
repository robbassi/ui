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
typedef SDL_Color UI_Color;

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
#define UI_MAX_ALIGN 1024
#define UI_MAX_STORAGE 10000

// UI Hash

// FNV-1a hash.
// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
u32 ui_hash(const void *data, size_t len) {
  const u8 *bytes = (const u8 *)data;
  u32 hash = 2166136261u;
  for (size_t i = 0; i < len; ++i) {
    hash ^= bytes[i];
    hash *= 16777619u;
  }
  return hash;
}

// UI Alignment Types

typedef enum {
  UI_ALIGN_LEFT,
  UI_ALIGN_RIGHT,
} UI_Align;

// UI Storage

typedef union {
  struct {
    UI_Align align;
    i32 start_index;
    Rect bounds;
    i32 x_offset;
  } align;
} UI_Data;

typedef struct {
  u32 id;
  UI_Data data;
} UI_StorageEntry;

UI_StorageEntry ui_storage[UI_MAX_STORAGE] = {0};

UI_Data *ui_get_data(u32 id) {
  for (i32 i = 0; i < UI_MAX_STORAGE; i++) {
    u32 index = (id + i) % UI_MAX_STORAGE;
    if (ui_storage[index].id == 0 || ui_storage[index].id == id) {
      ui_storage[index].id = id;
      return &ui_storage[index].data;
    }
    printf("Storage cache miss [%u]: 0x%x\n", index, id);
  }

  assert(false);
}

// UI Draw Command

typedef enum {
  UI_RECT,
  UI_BUTTON,
  UI_PANEL,
  UI_IMAGE,
} UI_DrawCmdType;

typedef struct {
  u32 id;
  UI_DrawCmdType type;
  Rect rect;
  void* image;
} UI_DrawCmd;

// UI Draw Queue

UI_DrawCmd ui_draw_queue[UI_MAX_DRAW_CMD] = {};
i32 ui_draw_queue_length = 0;

UI_DrawCmd *UI_PushDrawCmd() {
  assert(ui_draw_queue_length < UI_MAX_DRAW_CMD);

  return &ui_draw_queue[ui_draw_queue_length++];
}

// UI State

// UI Focus State

i32 ui_window_id = 0;
i32 ui_hover_id = 0;
i32 ui_active_id = 0;
// When true, the hover id can be overriden. Useful for overlapping buttons.
bool ui_hover_greedy = false;

// UI Input State

typedef enum {
  UI_MOUSE_BUTTON_LEFT = 1 << 0,
  UI_MOUSE_BUTTON_RIGHT = 1 << 1,
} UI_MouseButton;

typedef struct {
  v2 mouse_pos;
  u8 mouse_button_down;
  u8 mouse_button_up;
} UI_InputState;

const UI_InputState ui_default_input_state = {
  .mouse_pos = {0, 0},
  .mouse_button_down = 0,
  .mouse_button_up = 0,
};
UI_InputState ui_input_state = ui_default_input_state;

// UI Layout State

typedef enum {
  UI_LAYOUT_HORIZONTAL,
  UI_LAYOUT_VERTICAL,
} UI_Layout;

typedef struct {
  // A draw queue index. Useful for post processing of child cmds.
  i32 index;
  // The on screen position of the next widget.
  v2 pos;
  // The current layout type.
  UI_Layout layout;
  // The current bounds of visible widgets.
  Rect bounds;
  // The space between components.
  v2 margin;
  // The padding for components.
  v2 padding;
} UI_State;

const UI_State ui_default_state = {
  .index = 0,
  .pos = {0, 0},
  .layout = UI_LAYOUT_VERTICAL,
  .bounds = {0, 0, 0, 0},
  .margin = {10, 10},
  .padding = {10, 10},
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

void UI_UpdateLayout(Rect *rect) {
  switch (ui->layout) {
    case UI_LAYOUT_HORIZONTAL:
      ui->pos.x += rect->w + ui->margin.x;
      break;
    case UI_LAYOUT_VERTICAL:
      ui->pos.y += rect->h + ui->margin.y;
      break;
  }
  if (ui->bounds.x + ui->bounds.w < rect->x + rect->w) {
    ui->bounds.w = rect->x + rect->w - ui->bounds.x;
  }
  if (ui->bounds.y + ui->bounds.h < rect->y + rect->h) {
    ui->bounds.h = rect->y + rect->h - ui->bounds.y;
  }
}

// UI Utils

bool UI_MouseInRect(Rect *rect) {
  v2 *mouse_pos = &ui_input_state.mouse_pos;
  if (mouse_pos->x >= rect->x && mouse_pos->x <= rect->x + rect->w &&
      mouse_pos->y >= rect->y && mouse_pos->y <= rect->y + rect->h) {
    return true;
  }

  return false;
}

// UI Alignment
// typedef struct {
//   i32 start_index;
//   v2 start_pos;
//   UI_Align align;
// } UI_AlignInfo;
// 
const u8 *ui_align_stack[UI_MAX_ALIGN];
i32 ui_align_stack_length = 0;

void UI_BeginAlign(UI_Align align, const u8 *label) {
  ui_align_stack[ui_align_stack_length++] = label;
  u32 id = ui_hash(label, strlen(label));
  UI_Data *data = ui_get_data(id);
  data->align.start_index = ui_draw_queue_length;
  data->align.align = align;

  UI_PushState();
  ui->pos.x += data->align.x_offset;
  ui->bounds.w = 0;
  ui->bounds.h = 0;
}

void UI_EndAlign() {
  const u8 *label = ui_align_stack[--ui_align_stack_length];
  u32 id = ui_hash(label, strlen(label));
  UI_Data *data = ui_get_data(id);
  data->align.bounds = ui->bounds;
  UI_PopState();

  // TODO: Remove if, find a better way to stabilize.
  if (data->align.x_offset == 0) {
    switch (data->align.align) {
      case UI_ALIGN_LEFT: {
        i32 x = data->align.bounds.x;
        data->align.x_offset = ui->bounds.x - x;
      } break;
      case UI_ALIGN_RIGHT: {
        i32 x = data->align.bounds.x + data->align.bounds.w;
        data->align.x_offset = ui->bounds.x + ui->bounds.w - x;
      } break;
    }
  }
  //printf("h = %d\n", data->align.bounds.h);
  ui->bounds.h += data->align.bounds.h - ui->bounds.h;
}

// UI Widgets

// UI Rect

void UI_Rect(i32 w, i32 h) {
  UI_DrawCmd *cmd = UI_PushDrawCmd();
  cmd->type = UI_RECT;
  cmd->rect = (Rect){ui->pos.x, ui->pos.y, w, h};

  UI_UpdateLayout(&cmd->rect);
}

// UI Button

bool UI_Button(const char *label) {
  u32 id = ui_hash(label, strlen(label));
  UI_DrawCmd *cmd = UI_PushDrawCmd();
  cmd->id = id;
  cmd->type = UI_BUTTON;
  cmd->rect = (Rect){ui->pos.x, ui->pos.y, 100, 50};
  
  bool clicked = false;
  if (UI_MouseInRect(&cmd->rect)) {
    // Grab hover id if possible.
    if (ui_hover_id == 0 || ui_hover_greedy) {
      ui_hover_id = id;
    }

    // Set active if mouse down and hovered, and nothing else is active.
    if (ui_active_id == 0 && ui_hover_id == id && ui_input_state.mouse_button_down & UI_MOUSE_BUTTON_LEFT) {
      ui_active_id = id;
    }

    // Detect click, if active.
    if (ui_active_id == id && ui_input_state.mouse_button_up & UI_MOUSE_BUTTON_LEFT) {
      clicked = true;
      ui_active_id = 0;
    }

  } else {
    // Release hover id.
    if (ui_hover_id == id) {
      ui_hover_id = 0;
    }

    // Release active id, if mouse released outside of button.
    if (ui_active_id == id && ui_input_state.mouse_button_up & UI_MOUSE_BUTTON_LEFT) {
      ui_active_id = 0;
    }
  }

  UI_UpdateLayout(&cmd->rect);
  return clicked;
}

// UI Panel

void UI_BeginPanel() {
  UI_PushState();

  // Start of panel. Store the index, and create rect cmd, which we'll adjust
  // later in UI_EndPanel().
  ui->bounds.x = ui->pos.x;
  ui->bounds.y = ui->pos.y;
  ui->bounds.w = 0;
  ui->bounds.h = 0;
  ui->index = ui_draw_queue_length;
  ui->pos.x += ui->padding.x;
  ui->pos.y += ui->padding.y;
  {
    UI_DrawCmd *cmd = UI_PushDrawCmd();
    cmd->type = UI_PANEL;
  }
}

void UI_EndPanel() {
  UI_DrawCmd *cmd = &ui_draw_queue[ui->index];
  cmd->rect = ui->bounds;
  cmd->rect.w += ui->padding.x;
  cmd->rect.h += ui->padding.y;

  UI_PopState();
  UI_UpdateLayout(&cmd->rect);
}

// END UI library

// UI Renderer

void UI_Render() {
  for (i32 i = 0; i < ui_draw_queue_length; i++) {
    UI_DrawCmd *cmd = &ui_draw_queue[i];
    switch (cmd->type) {
      case UI_RECT:
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderFillRect(renderer, (Rect*)&cmd->rect);
        break;
      case UI_BUTTON:
        UI_Color color = {0, 0, 0, 255};
        if (ui_active_id == cmd->id) {
          color.b = 200;
        } else if (ui_hover_id == cmd->id) {
          color.b = 150;
        } else {
          color.b = 100;
        }
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer, (Rect*)&cmd->rect);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, (Rect*)&cmd->rect);
        break;
      case UI_PANEL:
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, (Rect*)&cmd->rect);
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
    ui_input_state.mouse_button_up = 0;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT:
          exit(0);
        case SDL_KEYDOWN:
          if (event.key.keysym.sym == SDLK_ESCAPE) {
            exit(0);
          }
          break;
        case SDL_MOUSEBUTTONDOWN:
          if (event.button.button == SDL_BUTTON(SDL_BUTTON_LEFT)) {
            ui_input_state.mouse_button_down |= UI_MOUSE_BUTTON_LEFT;
            ui_input_state.mouse_button_up &= ~UI_MOUSE_BUTTON_LEFT;
          }
          if (event.button.button == SDL_BUTTON(SDL_BUTTON_RIGHT)) {
            ui_input_state.mouse_button_down |= UI_MOUSE_BUTTON_RIGHT;
            ui_input_state.mouse_button_up &= ~UI_MOUSE_BUTTON_RIGHT;
          }
          break;
        case SDL_MOUSEBUTTONUP:
          if (event.button.button == SDL_BUTTON(SDL_BUTTON_LEFT)) {
            ui_input_state.mouse_button_up |= UI_MOUSE_BUTTON_LEFT;
            ui_input_state.mouse_button_down &= ~UI_MOUSE_BUTTON_LEFT;
          }
          if (event.button.button == SDL_BUTTON(SDL_BUTTON_RIGHT)) {
            ui_input_state.mouse_button_up |= UI_MOUSE_BUTTON_RIGHT;
            ui_input_state.mouse_button_down &= ~UI_MOUSE_BUTTON_RIGHT;
          }
          break;
      }
    }
    SDL_GetMouseState(&ui_input_state.mouse_pos.x, &ui_input_state.mouse_pos.y);

    // Update.
    {
      UI_Clear();

      UI_BeginPanel();
        UI_BeginPanel();
          ui->layout = UI_LAYOUT_VERTICAL;
          UI_Rect(100, 50);
          UI_Rect(100, 50);
          UI_Rect(100, 50);
        UI_EndPanel();
        UI_BeginPanel();
          ui->layout = UI_LAYOUT_HORIZONTAL;
          UI_Rect(200, 50);
          UI_Rect(200, 50);
          UI_Rect(200, 50);
          if(UI_Button("Ok")) {
            printf("Ok\n");
          }
          // Cause button to overlap.
          ui->pos.x -= 40;
          ui_hover_greedy = true;
          if(UI_Button("Cancel")) {
            printf("Cancel\n");
          }
          ui_hover_greedy = false;
        UI_EndPanel();
      UI_EndPanel();


      UI_BeginPanel();
        UI_Rect(500, 20);
        UI_BeginAlign(UI_ALIGN_LEFT, "Left Buttons");
          if(UI_Button("Cancel#")) {
            printf("Cancel\n");
          }
        UI_EndAlign();
        UI_BeginAlign(UI_ALIGN_RIGHT, "Right Buttons");
          ui->layout = UI_LAYOUT_HORIZONTAL;
          if(UI_Button("Ok#")) {
            printf("Ok\n");
          }
          if(UI_Button("Back#")) {
            printf("Back\n");
          }
        UI_EndAlign();
      UI_EndPanel();

    }

    // Render.
    {
      SDL_SetRenderDrawColor(renderer, 60, 80, 40, 255);
      SDL_RenderClear(renderer);
      UI_Render();
      SDL_RenderPresent(renderer);
    }

    // Throttle FPS.
    SDL_Delay(1000/16);
  }

  return EXIT_SUCCESS;
}
