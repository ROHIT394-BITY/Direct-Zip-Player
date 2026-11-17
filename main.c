#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "memory_manager.h"
#include "zip_engine.h"
#include "video_decoder.h"
#include "audio_player.h"
#include "player_controller.h"
#include "playlist.h"
#include "ui.h"

int main(int argc, char *argv[]) {
    printf("============================================================\n");
    printf("  DirectZip - In-Memory ZIP MP4 Video Player\n");
    printf("  Zero-Disk-Extraction Multimedia Engine\n");
    printf("============================================================\n\n");

    // 1. Initialize Memory Manager with 2GB limit
    if (!memory_manager_init(DIRECTZIP_DEFAULT_MAX_MEMORY)) {
        fprintf(stderr, "[Main] Failed to initialize memory manager\n");
        return 1;
    }

    // 2. Initialize Player Controller
    PlayerController *pc = player_controller_create();
    if (!pc) {
        fprintf(stderr, "[Main] Failed to create player controller\n");
        memory_manager_shutdown();
        return 1;
    }

    // 3. Initialize UI
    UIManager *ui = ui_init("DirectZip - In-Memory ZIP MP4 Video Player", 1280, 720);
    if (!ui) {
        fprintf(stderr, "[Main] Failed to initialize UI subsystem\n");
        player_controller_destroy(pc);
        memory_manager_shutdown();
        return 1;
    }

    // 4. If ZIP file passed via command-line, open it immediately
    if (argc > 1 && argv[1]) {
        printf("[Main] Auto-opening ZIP archive from arguments: %s\n", argv[1]);
        player_controller_open_zip(pc, argv[1]);
    }

    // 5. Main Application Loop
    bool running = true;
    Uint32 frame_start_time = 0;
    const Uint32 target_frame_delay = 1000 / 60; // 60 FPS target

    while (running) {
        frame_start_time = SDL_GetTicks();

        // Process inputs & window events
        if (!ui_handle_events(ui, pc)) {
            running = false;
            break;
        }

        // Update player clock, decode frames, and buffer audio
        player_controller_update(pc);

        // Render user interface
        ui_render(ui, pc);

        // Frame timing limiter
        Uint32 frame_duration = SDL_GetTicks() - frame_start_time;
        if (frame_duration < target_frame_delay) {
            SDL_Delay(target_frame_delay - frame_duration);
        }
    }

    printf("[Main] Shutting down DirectZip cleanly...\n");

    // 6. Clean release of all subsystems
    ui_destroy(ui);
    player_controller_destroy(pc);
    memory_manager_shutdown();

    printf("[Main] DirectZip shutdown completed successfully.\n");
    return 0;
}
