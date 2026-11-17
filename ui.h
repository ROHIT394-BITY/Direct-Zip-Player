#ifndef DIRECTZIP_UI_H
#define DIRECTZIP_UI_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "player_controller.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UIManager UIManager;

// Initialize the user interface subsystem, SDL window, and renderer
UIManager* ui_init(const char *title, int width, int height);

// Destroy UI and free SDL resources
void ui_destroy(UIManager *ui);

// Process SDL events (mouse clicks, drags, key presses, window resizing, drop file)
// Returns false when user requests application exit (SDL_QUIT or Alt+F4)
bool ui_handle_events(UIManager *ui, PlayerController *pc);

// Render the entire desktop interface (Top bar, Sidebar, Video viewport, Bottom controls)
void ui_render(UIManager *ui, PlayerController *pc);

// Toggle fullscreen mode
void ui_toggle_fullscreen(UIManager *ui);

// Native file open dialog (returns allocated path string, caller must free)
char* ui_open_file_dialog(void);

#ifdef __cplusplus
}
#endif

#endif // DIRECTZIP_UI_H
