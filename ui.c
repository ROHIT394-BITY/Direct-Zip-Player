#define SDL_MAIN_HANDLED
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#endif

// Built-in 8x16 font bitmap for ASCII 32 to 126
// Standard compact 8x16 VGA font matrix
static const uint8_t s_font8x16[95][16] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x18,0x3c,0x3c,0x3c,0x18,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00}, // '!'
    {0x00,0x66,0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // '"'
    {0x00,0x00,0x6c,0x6c,0xfe,0x6c,0x6c,0x6c,0xfe,0x6c,0x6c,0x00,0x00,0x00,0x00,0x00}, // '#'
    {0x18,0x18,0x7c,0xc6,0xc2,0xc0,0x7c,0x06,0x06,0x86,0xc6,0x7c,0x18,0x18,0x00,0x00}, // '$'
    {0x00,0x00,0x00,0x00,0xc2,0xc6,0x0c,0x18,0x30,0x60,0xc6,0x86,0x00,0x00,0x00,0x00}, // '%'
    {0x00,0x00,0x38,0x6c,0x64,0x68,0x30,0x76,0xcc,0xcc,0xcc,0x76,0x00,0x00,0x00,0x00}, // '&'
    {0x00,0x30,0x30,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // '''
    {0x00,0x0c,0x18,0x30,0x30,0x60,0x60,0x60,0x60,0x60,0x30,0x30,0x18,0x0c,0x00,0x00}, // '('
    {0x00,0x30,0x18,0x0c,0x0c,0x06,0x06,0x06,0x06,0x06,0x0c,0x0c,0x18,0x30,0x00,0x00}, // ')'
    {0x00,0x00,0x00,0x66,0x3c,0xff,0x3c,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // '*'
    {0x00,0x00,0x00,0x18,0x18,0x7e,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // '+'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x18,0x08,0x10,0x00,0x00}, // ','
    {0x00,0x00,0x00,0x00,0x00,0x7e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // '-'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00}, // '.'
    {0x00,0x02,0x06,0x0c,0x18,0x30,0x60,0xc0,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // '/'
    {0x00,0x00,0x7c,0xc6,0xce,0xde,0xf6,0xe6,0xc6,0xc6,0x7c,0x00,0x00,0x00,0x00,0x00}, // '0'
    {0x00,0x00,0x18,0x38,0x78,0x18,0x18,0x18,0x18,0x18,0x7e,0x00,0x00,0x00,0x00,0x00}, // '1'
    {0x00,0x00,0x7c,0xc6,0x06,0x0c,0x18,0x30,0x60,0xc0,0xfe,0x00,0x00,0x00,0x00,0x00}, // '2'
    {0x00,0x00,0x7c,0xc6,0x06,0x06,0x3c,0x06,0x06,0xc6,0x7c,0x00,0x00,0x00,0x00,0x00}, // '3'
    {0x00,0x00,0x0c,0x1c,0x3c,0x6c,0xcc,0xfe,0x0c,0x0c,0x1e,0x00,0x00,0x00,0x00,0x00}, // '4'
    {0x00,0x00,0xfe,0xc0,0xc0,0xfc,0x06,0x06,0x06,0xc6,0x7c,0x00,0x00,0x00,0x00,0x00}, // '5'
    {0x00,0x00,0x38,0x60,0xc0,0xfc,0xc6,0xc6,0xc6,0xc6,0x7c,0x00,0x00,0x00,0x00,0x00}, // '6'
    {0x00,0x00,0xfe,0xc6,0x06,0x0c,0x18,0x30,0x30,0x30,0x30,0x00,0x00,0x00,0x00,0x00}, // '7'
    {0x00,0x00,0x7c,0xc6,0xc6,0xc6,0x7c,0xc6,0xc6,0xc6,0x7c,0x00,0x00,0x00,0x00,0x00}, // '8'
    {0x00,0x00,0x7c,0xc6,0xc6,0xc6,0x7e,0x06,0x06,0x0c,0x78,0x00,0x00,0x00,0x00,0x00}, // '9'
    {0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00}, // ':'
    {0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x18,0x18,0x08,0x10,0x00,0x00,0x00}, // ';'
    {0x00,0x06,0x0c,0x18,0x30,0x60,0x30,0x18,0x0c,0x06,0x00,0x00,0x00,0x00,0x00,0x00}, // '<'
    {0x00,0x00,0x00,0x00,0x7e,0x00,0x00,0x7e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // '='
    {0x00,0x60,0x30,0x18,0x0c,0x06,0x0c,0x18,0x30,0x60,0x00,0x00,0x00,0x00,0x00,0x00}, // '>'
    {0x00,0x00,0x7c,0xc6,0x06,0x0c,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00}, // '?'
    {0x00,0x00,0x7c,0xc6,0xc6,0xde,0xde,0xde,0xd8,0xc0,0x7c,0x00,0x00,0x00,0x00,0x00}, // '@'
    {0x00,0x00,0x10,0x38,0x6c,0xc6,0xc6,0xfe,0xc6,0xc6,0xc6,0x00,0x00,0x00,0x00,0x00}, // 'A'
    {0x00,0x00,0xfc,0x66,0x66,0x7c,0x66,0x66,0x66,0x66,0xfc,0x00,0x00,0x00,0x00,0x00}, // 'B'
    {0x00,0x00,0x3c,0x66,0xc2,0xc0,0xc0,0xc0,0xc2,0x66,0x3c,0x00,0x00,0x00,0x00,0x00}, // 'C'
    {0x00,0x00,0xf8,0x6c,0x66,0x66,0x66,0x66,0x66,0x6c,0xf8,0x00,0x00,0x00,0x00,0x00}, // 'D'
    {0x00,0x00,0xfe,0x62,0x68,0x78,0x68,0x60,0x62,0x66,0xfe,0x00,0x00,0x00,0x00,0x00}, // 'E'
    {0x00,0x00,0xfe,0x62,0x68,0x78,0x68,0x60,0x60,0x60,0xf0,0x00,0x00,0x00,0x00,0x00}, // 'F'
    {0x00,0x00,0x3c,0x66,0xc2,0xc0,0xc0,0xde,0xc6,0x66,0x3a,0x00,0x00,0x00,0x00,0x00}, // 'G'
    {0x00,0x00,0xc6,0xc6,0xc6,0xc6,0xfe,0xc6,0xc6,0xc6,0xc6,0x00,0x00,0x00,0x00,0x00}, // 'H'
    {0x00,0x00,0x3c,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3c,0x00,0x00,0x00,0x00,0x00}, // 'I'
    {0x00,0x00,0x1e,0x0c,0x0c,0x0c,0x0c,0x0c,0xcc,0xcc,0x78,0x00,0x00,0x00,0x00,0x00}, // 'J'
    {0x00,0x00,0xe6,0x66,0x6c,0x78,0x78,0x6c,0x66,0x66,0xe6,0x00,0x00,0x00,0x00,0x00}, // 'K'
    {0x00,0x00,0xf0,0x60,0x60,0x60,0x60,0x60,0x62,0x66,0xfe,0x00,0x00,0x00,0x00,0x00}, // 'L'
    {0x00,0x00,0xc6,0xee,0xfe,0xd6,0xc6,0xc6,0xc6,0xc6,0xc6,0x00,0x00,0x00,0x00,0x00}, // 'M'
    {0x00,0x00,0xc6,0xe6,0xf6,0xfe,0xde,0xce,0xc6,0xc6,0xc6,0x00,0x00,0x00,0x00,0x00}, // 'N'
    {0x00,0x00,0x7c,0xc6,0xc6,0xc6,0xc6,0xc6,0xc6,0xc6,0x7c,0x00,0x00,0x00,0x00,0x00}, // 'O'
    {0x00,0x00,0xfc,0x66,0x66,0x66,0x7c,0x60,0x60,0x60,0xf0,0x00,0x00,0x00,0x00,0x00}, // 'P'
    {0x00,0x00,0x7c,0xc6,0xc6,0xc6,0xc6,0xc6,0xd6,0xde,0x7c,0x0e,0x00,0x00,0x00,0x00}, // 'Q'
    {0x00,0x00,0xfc,0x66,0x66,0x66,0x7c,0x6c,0x66,0x66,0xe6,0x00,0x00,0x00,0x00,0x00}, // 'R'
    {0x00,0x00,0x7c,0xc6,0xc6,0x60,0x38,0x0c,0xc6,0xc6,0x7c,0x00,0x00,0x00,0x00,0x00}, // 'S'
    {0x00,0x00,0x7e,0x7e,0x5a,0x18,0x18,0x18,0x18,0x18,0x3c,0x00,0x00,0x00,0x00,0x00}, // 'T'
    {0x00,0x00,0xc6,0xc6,0xc6,0xc6,0xc6,0xc6,0xc6,0xc6,0x7c,0x00,0x00,0x00,0x00,0x00}, // 'U'
    {0x00,0x00,0xc6,0xc6,0xc6,0xc6,0xc6,0x6c,0x6c,0x38,0x10,0x00,0x00,0x00,0x00,0x00}, // 'V'
    {0x00,0x00,0xc6,0xc6,0xc6,0xc6,0xd6,0xd6,0xfe,0x6c,0x6c,0x00,0x00,0x00,0x00,0x00}, // 'W'
    {0x00,0x00,0xc6,0xc6,0x6c,0x38,0x38,0x6c,0xc6,0xc6,0xc6,0x00,0x00,0x00,0x00,0x00}, // 'X'
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3c,0x18,0x18,0x18,0x3c,0x00,0x00,0x00,0x00,0x00}, // 'Y'
    {0x00,0x00,0xfe,0xc6,0x8c,0x18,0x30,0x60,0xc2,0xc6,0xfe,0x00,0x00,0x00,0x00,0x00}, // 'Z'
    {0x00,0x3c,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x3c,0x00,0x00,0x00}, // '['
    {0x00,0x80,0xc0,0x60,0x30,0x18,0x0c,0x06,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // '\'
    {0x00,0x3c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x3c,0x00,0x00,0x00}, // ']'
    {0x10,0x38,0x6c,0xc6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // '^'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0x00}, // '_'
    {0x30,0x30,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // '`'
    {0x00,0x00,0x00,0x00,0x78,0x0c,0x7c,0xcc,0xcc,0xcc,0x76,0x00,0x00,0x00,0x00,0x00}, // 'a'
    {0x00,0xe0,0x60,0x60,0x7c,0x66,0x66,0x66,0x66,0x66,0xdc,0x00,0x00,0x00,0x00,0x00}, // 'b'
    {0x00,0x00,0x00,0x00,0x7c,0xc6,0xc0,0xc0,0xc0,0xc6,0x7c,0x00,0x00,0x00,0x00,0x00}, // 'c'
    {0x00,0x1c,0x0c,0x0c,0x7c,0xcc,0xcc,0xcc,0xcc,0xcc,0x76,0x00,0x00,0x00,0x00,0x00}, // 'd'
    {0x00,0x00,0x00,0x00,0x7c,0xc6,0xfe,0xc0,0xc0,0xc6,0x7c,0x00,0x00,0x00,0x00,0x00}, // 'e'
    {0x00,0x38,0x6c,0x64,0x60,0xf0,0x60,0x60,0x60,0x60,0xf0,0x00,0x00,0x00,0x00,0x00}, // 'f'
    {0x00,0x00,0x00,0x00,0x76,0xcc,0xcc,0xcc,0x7c,0x0c,0xcc,0x78,0x00,0x00,0x00,0x00}, // 'g'
    {0x00,0xe0,0x60,0x60,0x6c,0x76,0x66,0x66,0x66,0x66,0xe6,0x00,0x00,0x00,0x00,0x00}, // 'h'
    {0x00,0x18,0x18,0x00,0x38,0x18,0x18,0x18,0x18,0x18,0x3c,0x00,0x00,0x00,0x00,0x00}, // 'i'
    {0x00,0x0c,0x0c,0x00,0x1c,0x0c,0x0c,0x0c,0x0c,0x0c,0xcc,0x78,0x00,0x00,0x00,0x00}, // 'j'
    {0x00,0xe0,0x60,0x60,0x66,0x6c,0x78,0x78,0x6c,0x66,0xe6,0x00,0x00,0x00,0x00,0x00}, // 'k'
    {0x00,0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3c,0x00,0x00,0x00,0x00,0x00}, // 'l'
    {0x00,0x00,0x00,0x00,0xec,0xfe,0xd6,0xd6,0xd6,0xd6,0xc6,0x00,0x00,0x00,0x00,0x00}, // 'm'
    {0x00,0x00,0x00,0x00,0xdc,0x66,0x66,0x66,0x66,0x66,0x66,0x00,0x00,0x00,0x00,0x00}, // 'n'
    {0x00,0x00,0x00,0x00,0x7c,0xc6,0xc6,0xc6,0xc6,0xc6,0x7c,0x00,0x00,0x00,0x00,0x00}, // 'o'
    {0x00,0x00,0x00,0x00,0xdc,0x66,0x66,0x66,0x7c,0x60,0x60,0xf0,0x00,0x00,0x00,0x00}, // 'p'
    {0x00,0x00,0x00,0x00,0x76,0xcc,0xcc,0xcc,0x7c,0x0c,0x0c,0x1e,0x00,0x00,0x00,0x00}, // 'q'
    {0x00,0x00,0x00,0x00,0xdc,0x76,0x66,0x60,0x60,0x60,0xf0,0x00,0x00,0x00,0x00,0x00}, // 'r'
    {0x00,0x00,0x00,0x00,0x7c,0xc6,0x70,0x38,0x1c,0xc6,0x7c,0x00,0x00,0x00,0x00,0x00}, // 's'
    {0x00,0x10,0x30,0x30,0xfc,0x30,0x30,0x30,0x30,0x34,0x18,0x00,0x00,0x00,0x00,0x00}, // 't'
    {0x00,0x00,0x00,0x00,0xcc,0xcc,0xcc,0xcc,0xcc,0xcc,0x76,0x00,0x00,0x00,0x00,0x00}, // 'u'
    {0x00,0x00,0x00,0x00,0xc6,0xc6,0xc6,0xc6,0x6c,0x38,0x10,0x00,0x00,0x00,0x00,0x00}, // 'v'
    {0x00,0x00,0x00,0x00,0xc6,0xc6,0xd6,0xd6,0xfe,0x6c,0x6c,0x00,0x00,0x00,0x00,0x00}, // 'w'
    {0x00,0x00,0x00,0x00,0xc6,0x6c,0x38,0x38,0x6c,0xc6,0xc6,0x00,0x00,0x00,0x00,0x00}, // 'x'
    {0x00,0x00,0x00,0x00,0xc6,0xc6,0xc6,0xc6,0x7e,0x06,0x0c,0xf8,0x00,0x00,0x00,0x00}, // 'y'
    {0x00,0x00,0x00,0x00,0xfe,0xcc,0x18,0x30,0x60,0xc6,0xfe,0x00,0x00,0x00,0x00,0x00}, // 'z'
    {0x00,0x0e,0x18,0x18,0x18,0x70,0x18,0x18,0x18,0x18,0x0e,0x00,0x00,0x00,0x00,0x00}, // '{'
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00}, // '|'
    {0x00,0x70,0x18,0x18,0x18,0x0e,0x18,0x18,0x18,0x18,0x70,0x00,0x00,0x00,0x00,0x00}, // '}'
    {0x76,0xdc,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}  // '~'
};

struct UIManager {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *video_texture;
    int tex_width;
    int tex_height;
    int tex_is_yuv;

    int win_width;
    int win_height;
    bool is_fullscreen;
    bool show_quality_menu;

    // Interactive state
    int mouse_x;
    int mouse_y;
    bool mouse_down;
    bool dragging_timeline;
    bool dragging_volume;
    int hovered_playlist_idx;

    // Layout dimensions
    int topbar_height;
    int bottombar_height;
    int sidebar_width;
    int playlist_scroll_offset;
};

static void draw_text(SDL_Renderer *ren, int x, int y, const char *str, SDL_Color color, float scale) {
    if (!str) return;
    int cur_x = x;
    int char_w = (int)(8 * scale);
    int char_h = (int)(16 * scale);

    SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, color.a);

    while (*str) {
        char c = *str++;
        if (c < 32 || c > 126) c = '?';
        int glyph_idx = c - 32;

        for (int row = 0; row < 16; row++) {
            uint8_t bits = s_font8x16[glyph_idx][row];
            if (!bits) continue;
            for (int col = 0; col < 8; col++) {
                if (bits & (0x80 >> col)) {
                    SDL_Rect r = {
                        cur_x + (int)(col * scale),
                        y + (int)(row * scale),
                        (int)(scale >= 1.0f ? scale : 1),
                        (int)(scale >= 1.0f ? scale : 1)
                    };
                    SDL_RenderFillRect(ren, &r);
                }
            }
        }
        cur_x += char_w;
    }
}

static void draw_rounded_rect(SDL_Renderer *ren, SDL_Rect *rect, SDL_Color color, int radius) {
    SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, color.a);
    if (radius <= 0) {
        SDL_RenderFillRect(ren, rect);
        return;
    }
    // Main body
    SDL_Rect body = { rect->x + radius, rect->y, rect->w - 2 * radius, rect->h };
    SDL_Rect side = { rect->x, rect->y + radius, rect->w, rect->h - 2 * radius };
    SDL_RenderFillRect(ren, &body);
    SDL_RenderFillRect(ren, &side);
}

char* ui_open_file_dialog(void) {
#if defined(_WIN32)
    wchar_t filename[1024] = {0};
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = L"ZIP Archives (*.zip)\0*.zip\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof(filename) / sizeof(wchar_t);
    ofn.lpstrTitle = L"DirectZip - Select ZIP Video Archive";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        int utf8_len = WideCharToMultiByte(CP_UTF8, 0, filename, -1, NULL, 0, NULL, NULL);
        if (utf8_len > 0) {
            char *utf8_path = (char *)malloc(utf8_len);
            WideCharToMultiByte(CP_UTF8, 0, filename, -1, utf8_path, utf8_len, NULL, NULL);
            return utf8_path;
        }
    }
#endif
    return NULL;
}

UIManager* ui_init(const char *title, int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "[UI] SDL_Init failed: %s\n", SDL_GetError());
        return NULL;
    }

    UIManager *ui = (UIManager *)calloc(1, sizeof(UIManager));
    if (!ui) return NULL;

    ui->win_width = width > 0 ? width : 1280;
    ui->win_height = height > 0 ? height : 720;
    ui->topbar_height = 54;
    ui->bottombar_height = 80;
    ui->sidebar_width = 330;
    ui->is_fullscreen = false;

    ui->window = SDL_CreateWindow(
        title ? title : "DirectZip - In-Memory Video Player",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        ui->win_width, ui->win_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!ui->window) {
        fprintf(stderr, "[UI] SDL_CreateWindow failed: %s\n", SDL_GetError());
        free(ui);
        return NULL;
    }

    ui->renderer = SDL_CreateRenderer(
        ui->window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!ui->renderer) {
        ui->renderer = SDL_CreateRenderer(ui->window, -1, 0);
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    return ui;
}

void ui_destroy(UIManager *ui) {
    if (!ui) return;
    if (ui->video_texture) {
        SDL_DestroyTexture(ui->video_texture);
    }
    if (ui->renderer) {
        SDL_DestroyRenderer(ui->renderer);
    }
    if (ui->window) {
        SDL_DestroyWindow(ui->window);
    }
    SDL_Quit();
    free(ui);
}

void ui_toggle_fullscreen(UIManager *ui) {
    if (!ui || !ui->window) return;
    ui->is_fullscreen = !ui->is_fullscreen;
    SDL_SetWindowFullscreen(ui->window, ui->is_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

static void format_time_str(double sec, char *out, size_t max_len) {
    if (sec < 0.0) sec = 0.0;
    int total_sec = (int)sec;
    int m = total_sec / 60;
    int s = total_sec % 60;
    int h = m / 60;
    if (h > 0) {
        snprintf(out, max_len, "%02d:%02d:%02d", h, m % 60, s);
    } else {
        snprintf(out, max_len, "%02d:%02d", m, s);
    }
}

bool ui_handle_events(UIManager *ui, PlayerController *pc) {
    if (!ui || !pc) return false;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                return false;

            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_RESIZED ||
                    e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    ui->win_width = e.window.data1;
                    ui->win_height = e.window.data2;
                }
                break;

            case SDL_DROPFILE: {
                char *dropped_path = e.drop.file;
                if (dropped_path) {
                    player_controller_open_zip(pc, dropped_path);
                    SDL_free(dropped_path);
                }
                break;
            }

            case SDL_MOUSEMOTION:
                ui->mouse_x = e.motion.x;
                ui->mouse_y = e.motion.y;

                if (ui->dragging_timeline) {
                    int tl_x = ui->sidebar_width + 120;
                    int tl_w = ui->win_width - ui->sidebar_width - 240;
                    if (tl_w > 0) {
                        double frac = (double)(ui->mouse_x - tl_x) / (double)tl_w;
                        if (frac < 0.0) frac = 0.0;
                        if (frac > 1.0) frac = 1.0;
                        double dur = player_controller_get_duration(pc);
                        player_controller_seek_to(pc, frac * dur);
                    }
                } else if (ui->dragging_volume) {
                    int vol_x = ui->win_width - 150;
                    int vol_w = 70;
                    double frac = (double)(ui->mouse_x - vol_x) / (double)vol_w;
                    if (frac < 0.0) frac = 0.0;
                    if (frac > 1.0) frac = 1.0;
                    player_controller_set_volume(pc, (float)frac);
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    ui->mouse_down = true;
                    int mx = e.button.x;
                    int my = e.button.y;

                    // If Video Quality Modal is open, handle modal clicks exclusively
                    if (ui->show_quality_menu) {
                        int mw = 520, mh = 380;
                        int qx = (ui->win_width - mw) / 2;
                        int qy = (ui->win_height - mh) / 2;

                        // Close button [X]
                        if (mx >= qx + mw - 40 && mx <= qx + mw - 10 && my >= qy + 10 && my <= qy + 36) {
                            ui->show_quality_menu = false;
                            break;
                        }

                        // Mode Option 1: Automatic 1080p
                        if (mx >= qx + 20 && mx <= qx + mw - 20 && my >= qy + 65 && my <= qy + 90) {
                            player_controller_set_upscaling_mode(pc, UPSCALING_MODE_AUTO_1080P);
                            break;
                        }
                        // Mode Option 2: Original Resolution
                        if (mx >= qx + 20 && mx <= qx + mw - 20 && my >= qy + 95 && my <= qy + 120) {
                            player_controller_set_upscaling_mode(pc, UPSCALING_MODE_ORIGINAL);
                            break;
                        }
                        // Mode Option 3: Automatic Best Fit
                        if (mx >= qx + 20 && mx <= qx + mw - 20 && my >= qy + 125 && my <= qy + 150) {
                            player_controller_set_upscaling_mode(pc, UPSCALING_MODE_BEST_FIT);
                            break;
                        }

                        // Quality Option 1: High Quality (Lanczos)
                        if (mx >= qx + 20 && mx <= qx + 170 && my >= qy + 185 && my <= qy + 215) {
                            player_controller_set_upscaling_quality(pc, UPSCALING_QUALITY_HIGH);
                            break;
                        }
                        // Quality Option 2: Balanced (Bicubic)
                        if (mx >= qx + 180 && mx <= qx + 330 && my >= qy + 185 && my <= qy + 215) {
                            player_controller_set_upscaling_quality(pc, UPSCALING_QUALITY_BALANCED);
                            break;
                        }
                        // Quality Option 3: Performance (Bilinear)
                        if (mx >= qx + 340 && mx <= qx + 490 && my >= qy + 185 && my <= qy + 215) {
                            player_controller_set_upscaling_quality(pc, UPSCALING_QUALITY_PERFORMANCE);
                            break;
                        }

                        // Dismiss button at bottom
                        if (mx >= qx + (mw - 140) / 2 && mx <= qx + (mw + 140) / 2 && my >= qy + mh - 42 && my <= qy + mh - 12) {
                            ui->show_quality_menu = false;
                            break;
                        }

                        // Click outside modal dismisses
                        if (mx < qx || mx > qx + mw || my < qy || my > qy + mh) {
                            ui->show_quality_menu = false;
                        }
                        break;
                    }

                    // Top Bar Open Button
                    if (mx >= 180 && mx <= 300 && my >= 10 && my <= 44) {
                        char *path = ui_open_file_dialog();
                        if (path) {
                            player_controller_open_zip(pc, path);
                            free(path);
                        }
                    }

                    // Top Bar Diagnostics Button
                    if (mx >= 310 && mx <= 390 && my >= 10 && my <= 44) {
                        player_controller_toggle_diagnostics(pc);
                    }

                    // Top Bar Video Quality Button
                    if (mx >= 400 && mx <= 520 && my >= 10 && my <= 44) {
                        ui->show_quality_menu = !ui->show_quality_menu;
                    }

                    // Playlist clicks
                    Playlist *pl = player_controller_get_playlist(pc);
                    if (pl && mx < ui->sidebar_width && my > ui->topbar_height && my < ui->win_height - ui->bottombar_height) {
                        int item_y = ui->topbar_height + 40;
                        int count = playlist_get_count(pl);
                        for (int i = 0; i < count; i++) {
                            if (my >= item_y && my <= item_y + 44) {
                                player_controller_load_track(pc, i);
                                break;
                            }
                            item_y += 50;
                        }
                    }

                    // Bottom controls
                    if (my >= ui->win_height - ui->bottombar_height) {
                        // Timeline click
                        int tl_x = ui->sidebar_width + 120;
                        int tl_w = ui->win_width - ui->sidebar_width - 240;
                        if (mx >= tl_x && mx <= tl_x + tl_w && my >= ui->win_height - ui->bottombar_height + 10 && my <= ui->win_height - ui->bottombar_height + 30) {
                            ui->dragging_timeline = true;
                            double frac = (double)(mx - tl_x) / (double)tl_w;
                            double dur = player_controller_get_duration(pc);
                            player_controller_seek_to(pc, frac * dur);
                        }

                        // Play/Pause button
                        int ctrl_y = ui->win_height - ui->bottombar_height + 38;
                        int play_btn_x = ui->sidebar_width + 70;
                        if (mx >= play_btn_x && mx <= play_btn_x + 36 && my >= ctrl_y && my <= ctrl_y + 34) {
                            player_controller_toggle_play_pause(pc);
                        }

                        // Prev button
                        if (mx >= play_btn_x - 45 && mx <= play_btn_x - 10 && my >= ctrl_y && my <= ctrl_y + 34) {
                            player_controller_prev(pc);
                        }

                        // Next button
                        if (mx >= play_btn_x + 45 && mx <= play_btn_x + 80 && my >= ctrl_y && my <= ctrl_y + 34) {
                            player_controller_next(pc);
                        }

                        // Seek -10s
                        if (mx >= play_btn_x + 90 && mx <= play_btn_x + 130 && my >= ctrl_y && my <= ctrl_y + 34) {
                            player_controller_seek_relative(pc, -10.0);
                        }

                        // Seek +10s
                        if (mx >= play_btn_x + 135 && mx <= play_btn_x + 175 && my >= ctrl_y && my <= ctrl_y + 34) {
                            player_controller_seek_relative(pc, 10.0);
                        }

                        // Volume slider / mute
                        int vol_btn_x = ui->win_width - 190;
                        if (mx >= vol_btn_x && mx <= vol_btn_x + 30 && my >= ctrl_y && my <= ctrl_y + 34) {
                            player_controller_toggle_mute(pc);
                        }
                        if (mx >= ui->win_width - 150 && mx <= ui->win_width - 80 && my >= ctrl_y && my <= ctrl_y + 34) {
                            ui->dragging_volume = true;
                            double frac = (double)(mx - (ui->win_width - 150)) / 70.0;
                            player_controller_set_volume(pc, (float)frac);
                        }

                        // Fullscreen button
                        if (mx >= ui->win_width - 65 && mx <= ui->win_width - 20 && my >= ctrl_y && my <= ctrl_y + 34) {
                            ui_toggle_fullscreen(ui);
                        }
                    }

                    // Center video viewport click toggles play/pause
                    if (mx >= ui->sidebar_width && my >= ui->topbar_height && my < ui->win_height - ui->bottombar_height) {
                        player_controller_toggle_play_pause(pc);
                    }
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    ui->mouse_down = false;
                    ui->dragging_timeline = false;
                    ui->dragging_volume = false;
                }
                break;

            case SDL_KEYDOWN:
                switch (e.key.keysym.sym) {
                    case SDLK_d:
                        player_controller_toggle_diagnostics(pc);
                        break;
                    case SDLK_q:
                        ui->show_quality_menu = !ui->show_quality_menu;
                        break;
                    case SDLK_u:
                        player_controller_set_upscaling_mode(pc, (player_controller_get_upscaling_mode(pc) + 1) % 3);
                        break;
                    case SDLK_k:
                        player_controller_set_upscaling_quality(pc, (player_controller_get_upscaling_quality(pc) + 1) % 3);
                        break;
                    case SDLK_SPACE:
                        player_controller_toggle_play_pause(pc);
                        break;
                    case SDLK_LEFT:
                        player_controller_seek_relative(pc, -5.0);
                        break;
                    case SDLK_RIGHT:
                        player_controller_seek_relative(pc, 5.0);
                        break;
                    case SDLK_UP:
                        player_controller_set_volume(pc, player_controller_get_volume(pc) + 0.05f);
                        break;
                    case SDLK_DOWN:
                        player_controller_set_volume(pc, player_controller_get_volume(pc) - 0.05f);
                        break;
                    case SDLK_m:
                        player_controller_toggle_mute(pc);
                        break;
                    case SDLK_f:
                    case SDLK_F11:
                        ui_toggle_fullscreen(ui);
                        break;
                    case SDLK_ESCAPE:
                        if (ui->is_fullscreen) ui_toggle_fullscreen(ui);
                        break;
                    case SDLK_n:
                        player_controller_next(pc);
                        break;
                    case SDLK_p:
                        player_controller_prev(pc);
                        break;
                    case SDLK_o: {
                        char *p = ui_open_file_dialog();
                        if (p) {
                            player_controller_open_zip(pc, p);
                            free(p);
                        }
                        break;
                    }
                }
                break;
        }
    }

    return true;
}

void ui_render(UIManager *ui, PlayerController *pc) {
    if (!ui || !ui->renderer || !pc) return;

    SDL_Renderer *ren = ui->renderer;

    // Background: deep dark slate
    SDL_SetRenderDrawColor(ren, 15, 17, 26, 255);
    SDL_RenderClear(ren);

    // ==========================================
    // 1. TOP NAVIGATION BAR
    // ==========================================
    SDL_Rect topbar = { 0, 0, ui->win_width, ui->topbar_height };
    SDL_Color topbar_col = { 23, 25, 35, 255 };
    draw_rounded_rect(ren, &topbar, topbar_col, 0);

    // Bottom border for topbar
    SDL_SetRenderDrawColor(ren, 38, 42, 60, 255);
    SDL_RenderDrawLine(ren, 0, ui->topbar_height - 1, ui->win_width, ui->topbar_height - 1);

    // DirectZip Logo & App Name
    SDL_Color logo_col = { 59, 130, 246, 255 };
    SDL_Color white_col = { 248, 250, 252, 255 };
    SDL_Color gray_col = { 148, 163, 184, 255 };
    SDL_Color dark_gray = { 71, 85, 105, 255 };

    draw_text(ren, 16, 18, "DIRECTZIP", logo_col, 1.25f);
    draw_text(ren, 105, 20, "v1.0", gray_col, 0.9f);

    // Open ZIP Button
    bool open_hover = (ui->mouse_x >= 180 && ui->mouse_x <= 300 && ui->mouse_y >= 10 && ui->mouse_y <= 44);
    SDL_Rect open_btn = { 180, 10, 120, 34 };
    SDL_Color open_col = open_hover ? (SDL_Color){ 37, 99, 235, 255 } : (SDL_Color){ 30, 58, 138, 255 };
    draw_rounded_rect(ren, &open_btn, open_col, 4);
    draw_text(ren, 192, 20, "[+] Open ZIP", white_col, 1.0f);

    // Diagnostics Toggle Button
    bool diag_hover = (ui->mouse_x >= 310 && ui->mouse_x <= 390 && ui->mouse_y >= 10 && ui->mouse_y <= 44);
    bool diag_active = player_controller_is_diagnostics_enabled(pc);
    SDL_Rect diag_btn = { 310, 10, 80, 34 };
    SDL_Color diag_col = diag_active ? (SDL_Color){ 16, 185, 129, 255 } :
                         diag_hover  ? (SDL_Color){ 51, 65, 85, 255 } :
                                       (SDL_Color){ 30, 41, 59, 255 };
    draw_rounded_rect(ren, &diag_btn, diag_col, 4);
    draw_text(ren, 324, 20, diag_active ? "[D] ON" : "[D] OFF", white_col, 0.95f);

    // Video Quality & 1080p Upscaler Button [Q]
    bool q_hover = (ui->mouse_x >= 400 && ui->mouse_x <= 520 && ui->mouse_y >= 10 && ui->mouse_y <= 44);
    VideoUpscalerInfo u_info = player_controller_get_upscaler_info(pc);
    bool u_active = u_info.is_active;
    SDL_Rect q_btn = { 400, 10, 120, 34 };
    SDL_Color q_col = ui->show_quality_menu ? (SDL_Color){ 245, 158, 11, 255 } :
                      u_active              ? (SDL_Color){ 16, 185, 129, 255 } :
                      q_hover               ? (SDL_Color){ 51, 65, 85, 255 } :
                                              (SDL_Color){ 30, 41, 59, 255 };
    draw_rounded_rect(ren, &q_btn, q_col, 4);
    char q_btn_txt[32];
    if (u_info.mode == UPSCALING_MODE_AUTO_1080P) {
        snprintf(q_btn_txt, sizeof(q_btn_txt), u_active ? "[Q] 1080p ON" : "[Q] 1080p OK");
    } else if (u_info.mode == UPSCALING_MODE_BEST_FIT) {
        snprintf(q_btn_txt, sizeof(q_btn_txt), "[Q] Fit Mode");
    } else {
        snprintf(q_btn_txt, sizeof(q_btn_txt), "[Q] Original");
    }
    draw_text(ren, 410, 20, q_btn_txt, white_col, 0.95f);

    // Resolution Badge (1080p FHD / 720p HD Output & Upscaling status)
    const char *res_lbl = player_controller_get_resolution_label(pc);
    char res_buf[64];
    if (u_info.original_width > 0 && u_info.original_height > 0) {
        if (u_active) {
            snprintf(res_buf, sizeof(res_buf), "[ %dx%d -> %dx%d | %s ]",
                     u_info.original_width, u_info.original_height,
                     u_info.output_width, u_info.output_height,
                     u_info.algorithm_name);
        } else {
            snprintf(res_buf, sizeof(res_buf), "[ %dx%d %s | Native ]",
                     u_info.original_width, u_info.original_height, res_lbl);
        }
    } else {
        snprintf(res_buf, sizeof(res_buf), "[ Auto 1080p Ready ]");
    }
    draw_text(ren, ui->win_width - 520, 20, res_buf, (SDL_Color){ 52, 211, 153, 255 }, 0.9f);

    // Memory Usage Badge
    MemoryManagerStats mem = memory_manager_get_stats();
    char mem_str[64];
    snprintf(mem_str, sizeof(mem_str), "RAM: %.1f MB / %.0f MB",
             (double)mem.currently_allocated_bytes / (1024.0 * 1024.0),
             (double)mem.max_allowed_bytes / (1024.0 * 1024.0));
    draw_text(ren, ui->win_width - 240, 20, mem_str, (SDL_Color){ 96, 165, 250, 255 }, 0.9f);

    // ==========================================
    // 2. LEFT SIDEBAR: PLAYLIST & ARCHIVE INFO
    // ==========================================
    SDL_Rect sidebar = { 0, ui->topbar_height, ui->sidebar_width, ui->win_height - ui->topbar_height - ui->bottombar_height };
    SDL_Color sidebar_col = { 19, 21, 31, 255 };
    draw_rounded_rect(ren, &sidebar, sidebar_col, 0);

    // Right border of sidebar
    SDL_SetRenderDrawColor(ren, 38, 42, 60, 255);
    SDL_RenderDrawLine(ren, ui->sidebar_width - 1, ui->topbar_height, ui->sidebar_width - 1, ui->win_height - ui->bottombar_height);

    const ZipArchive *arch = player_controller_get_archive(pc);
    Playlist *pl = player_controller_get_playlist(pc);

    if (arch) {
        const char *arch_name = strrchr(zip_engine_get_path(arch), '/');
        if (!arch_name) arch_name = strrchr(zip_engine_get_path(arch), '\\');
        arch_name = arch_name ? arch_name + 1 : zip_engine_get_path(arch);

        char title_buf[64];
        snprintf(title_buf, sizeof(title_buf), "%.24s", arch_name);
        draw_text(ren, 16, ui->topbar_height + 12, title_buf, white_col, 1.0f);

        char count_buf[32];
        snprintf(count_buf, sizeof(count_buf), "%d MP4 videos", playlist_get_count(pl));
        draw_text(ren, ui->sidebar_width - 110, ui->topbar_height + 12, count_buf, gray_col, 0.85f);
    } else {
        draw_text(ren, 16, ui->topbar_height + 12, "NO ARCHIVE LOADED", gray_col, 0.9f);
    }

    // Render playlist items
    if (pl && playlist_get_count(pl) > 0) {
        int item_y = ui->topbar_height + 40;
        int count = playlist_get_count(pl);
        int selected = playlist_get_selected_index(pl);

        for (int i = 0; i < count; i++) {
            const PlaylistItem *item = playlist_get_item(pl, i);
            if (!item) continue;

            bool is_sel = (i == selected);
            bool is_hov = (ui->mouse_x < ui->sidebar_width && ui->mouse_y >= item_y && ui->mouse_y <= item_y + 44);

            SDL_Rect card = { 10, item_y, ui->sidebar_width - 20, 44 };
            SDL_Color card_bg = is_sel ? (SDL_Color){ 30, 41, 59, 255 } :
                                is_hov ? (SDL_Color){ 24, 28, 42, 255 } :
                                         (SDL_Color){ 19, 21, 31, 255 };
            draw_rounded_rect(ren, &card, card_bg, 4);

            if (is_sel) {
                // Active indicator bar
                SDL_Rect bar = { 10, item_y, 4, 44 };
                draw_rounded_rect(ren, &bar, logo_col, 2);
            }

            // Filename
            char name_buf[40];
            snprintf(name_buf, sizeof(name_buf), "%d. %.22s", i + 1, item->display_name);
            draw_text(ren, 22, item_y + 8, name_buf, is_sel ? (SDL_Color){ 147, 197, 253, 255 } : white_col, 0.95f);

            // Size in MB
            char meta_buf[32];
            snprintf(meta_buf, sizeof(meta_buf), "%.1f MB", (double)item->uncompressed_size / (1024.0 * 1024.0));
            draw_text(ren, 22, item_y + 26, meta_buf, dark_gray, 0.75f);

            // Duration
            if (item->duration_sec > 0.0) {
                char dur_buf[16];
                format_time_str(item->duration_sec, dur_buf, sizeof(dur_buf));
                draw_text(ren, ui->sidebar_width - 70, item_y + 26, dur_buf, dark_gray, 0.75f);
            }

            item_y += 50;
            if (item_y > ui->win_height - ui->bottombar_height - 44) break;
        }
    } else {
        draw_text(ren, 24, ui->topbar_height + 60, "Playlist is empty.", dark_gray, 0.9f);
        draw_text(ren, 24, ui->topbar_height + 84, "Open a ZIP archive with", dark_gray, 0.85f);
        draw_text(ren, 24, ui->topbar_height + 104, "MP4 video files to begin.", dark_gray, 0.85f);
    }

    // ==========================================
    // 3. CENTRAL PLAYBACK AREA (VIEWPORT)
    // ==========================================
    int vp_x = ui->sidebar_width;
    int vp_y = ui->topbar_height;
    int vp_w = ui->win_width - ui->sidebar_width;
    int vp_h = ui->win_height - ui->topbar_height - ui->bottombar_height;

    PlayerState state = player_controller_get_state(pc);

    // Upload & Render Video Frame (Direct Hardware YUV420P Presentation)
    DecodedVideoFrame v_frame = {0};
    if (player_controller_lock_video_frame(pc, &v_frame)) {
        int f_w = v_frame.width;
        int f_h = v_frame.height;
        bool is_yuv = (v_frame.is_yuv != 0);

        if (!ui->video_texture || ui->tex_width != f_w || ui->tex_height != f_h || ui->tex_is_yuv != (int)is_yuv) {
            if (ui->video_texture) SDL_DestroyTexture(ui->video_texture);
            ui->video_texture = SDL_CreateTexture(
                ren,
                is_yuv ? SDL_PIXELFORMAT_IYUV : SDL_PIXELFORMAT_RGB24,
                SDL_TEXTUREACCESS_STREAMING,
                f_w, f_h
            );
            ui->tex_width = f_w;
            ui->tex_height = f_h;
            ui->tex_is_yuv = (int)is_yuv;
        }

        if (ui->video_texture) {
            if (is_yuv && v_frame.y_plane && v_frame.u_plane && v_frame.v_plane) {
                SDL_UpdateYUVTexture(
                    ui->video_texture, NULL,
                    v_frame.y_plane, v_frame.y_pitch,
                    v_frame.u_plane, v_frame.u_pitch,
                    v_frame.v_plane, v_frame.v_pitch
                );
            } else if (v_frame.data) {
                SDL_UpdateTexture(ui->video_texture, NULL, v_frame.data, v_frame.linesize);
            }

            // Compute letterboxing/pillarboxing maintaining native aspect ratio
            double aspect = (double)f_w / (double)f_h;
            int dst_w = vp_w;
            int dst_h = (int)(dst_w / aspect);
            if (dst_h > vp_h) {
                dst_h = vp_h;
                dst_w = (int)(dst_h * aspect);
            }
            int dst_x = vp_x + (vp_w - dst_w) / 2;
            int dst_y = vp_y + (vp_h - dst_h) / 2;

            SDL_Rect dst_rect = { dst_x, dst_y, dst_w, dst_h };
            SDL_RenderCopy(ren, ui->video_texture, NULL, &dst_rect);
        }
        player_controller_unlock_frame(pc);
    }

    // Real-Time Diagnostic HUD Overlay (Toggled via 'D' or [Diag] button)
    if (player_controller_is_diagnostics_enabled(pc)) {
        PlayerDiagnostics diag = player_controller_get_diagnostics(pc);

        int diag_w = 430;
        int diag_h = 280;
        int diag_x = vp_x + 16;
        int diag_y = vp_y + 16;

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_Rect diag_rect = { diag_x, diag_y, diag_w, diag_h };
        SDL_SetRenderDrawColor(ren, 10, 15, 26, 230);
        SDL_RenderFillRect(ren, &diag_rect);

        SDL_SetRenderDrawColor(ren, 59, 130, 246, 255);
        SDL_RenderDrawRect(ren, &diag_rect);

        SDL_Color diag_title = { 147, 197, 253, 255 };
        SDL_Color diag_green = { 52, 211, 153, 255 };
        SDL_Color diag_text  = { 241, 245, 249, 255 };
        SDL_Color diag_sub   = { 148, 163, 184, 255 };

        draw_text(ren, diag_x + 14, diag_y + 12, "DIRECTZIP MULTIMEDIA DIAGNOSTICS", diag_title, 0.95f);
        SDL_SetRenderDrawColor(ren, 38, 42, 60, 255);
        SDL_RenderDrawLine(ren, diag_x + 10, diag_y + 32, diag_x + diag_w - 10, diag_y + 32);

        char buf[128];
        int ly = diag_y + 40;

        const char *res_lbl = player_controller_get_resolution_label(pc);
        if (diag.upscaling_active) {
            snprintf(buf, sizeof(buf), "Resolution  : %dx%d -> %dx%d (%.2fx)",
                     diag.native_width, diag.native_height,
                     diag.output_width, diag.output_height,
                     diag.scale_factor);
            draw_text(ren, diag_x + 14, ly, buf, diag_text, 0.85f);
            ly += 20;

            snprintf(buf, sizeof(buf), "Upscaler    : %s (%s) [ACTIVE]",
                     diag.upscaling_algorithm ? diag.upscaling_algorithm : "SwsScale",
                     diag.upscaling_mode_name ? diag.upscaling_mode_name : "Auto 1080p");
            draw_text(ren, diag_x + 14, ly, buf, diag_green, 0.85f);
            ly += 20;
        } else {
            snprintf(buf, sizeof(buf), "Resolution  : %dx%d (%s Native)",
                     diag.native_width, diag.native_height, res_lbl);
            draw_text(ren, diag_x + 14, ly, buf, diag_text, 0.85f);
            ly += 20;

            snprintf(buf, sizeof(buf), "Upscaler    : %s (Pass-Through) [BYPASS]",
                     diag.upscaling_mode_name ? diag.upscaling_mode_name : "Original");
            draw_text(ren, diag_x + 14, ly, buf, diag_sub, 0.85f);
            ly += 20;
        }

        snprintf(buf, sizeof(buf), "Playback Spd: %.1fx Normal (A-V Synced)", diag.playback_speed);
        draw_text(ren, diag_x + 14, ly, buf, diag_green, 0.85f);
        ly += 20;

        snprintf(buf, sizeof(buf), "Frame Rate  : Orig: %.2f FPS | Render: %.1f FPS",
                 diag.original_fps, diag.current_fps);
        draw_text(ren, diag_x + 14, ly, buf, diag_text, 0.85f);
        ly += 20;

        const char *clock_src = diag.is_audio_master ? "Audio Clock (Hardware Master)" : "Monotonic Video Timer";
        snprintf(buf, sizeof(buf), "Master Clock: %s", clock_src);
        draw_text(ren, diag_x + 14, ly, buf, diag_text, 0.85f);
        ly += 20;

        snprintf(buf, sizeof(buf), "Clocks      : Audio: %.3fs | Video PTS: %.3fs",
                 diag.audio_clock, diag.video_clock);
        draw_text(ren, diag_x + 14, ly, buf, diag_text, 0.85f);
        ly += 20;

        const char *sync_status = (fabs(diag.av_sync_diff_ms) < 40.0) ? "In Sync" :
                                  (diag.av_sync_diff_ms > 0 ? "Video Leads" : "Audio Leads");
        snprintf(buf, sizeof(buf), "A-V Diff    : %+.1f ms (%s)", diag.av_sync_diff_ms, sync_status);
        draw_text(ren, diag_x + 14, ly, buf, fabs(diag.av_sync_diff_ms) < 50.0 ? diag_green : (SDL_Color){251, 146, 60, 255}, 0.85f);
        ly += 20;

        snprintf(buf, sizeof(buf), "Time Base   : %d / %d", diag.time_base_num, diag.time_base_den);
        draw_text(ren, diag_x + 14, ly, buf, diag_sub, 0.85f);
        ly += 20;

        snprintf(buf, sizeof(buf), "Frames      : Decoded: %llu | Dropped: %llu",
                 (unsigned long long)diag.decoded_frames, (unsigned long long)diag.dropped_frames);
        draw_text(ren, diag_x + 14, ly, buf, diag_sub, 0.85f);
        ly += 20;

        snprintf(buf, sizeof(buf), "Rendering   : Direct GPU YUV420P (Zero-Copy)");
        draw_text(ren, diag_x + 14, ly, buf, diag_green, 0.82f);
    }

    // Overlay state indications
    if (state == PLAYER_STATE_DECOMPRESSING) {
        // Dark translucent overlay
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 15, 17, 26, 200);
        SDL_Rect over = { vp_x, vp_y, vp_w, vp_h };
        SDL_RenderFillRect(ren, &over);

        // Decompression modal card
        int card_w = 380;
        int card_h = 130;
        int card_x = vp_x + (vp_w - card_w) / 2;
        int card_y = vp_y + (vp_h - card_h) / 2;

        SDL_Rect modal = { card_x, card_y, card_w, card_h };
        SDL_Color modal_col = { 30, 41, 59, 255 };
        draw_rounded_rect(ren, &modal, modal_col, 8);

        draw_text(ren, card_x + 20, card_y + 20, "IN-MEMORY DECOMPRESSION", (SDL_Color){ 147, 197, 253, 255 }, 1.0f);
        draw_text(ren, card_x + 20, card_y + 44, player_controller_get_status_text(pc), white_col, 0.85f);

        // Progress bar track
        float prog = player_controller_get_decompression_progress(pc);
        SDL_Rect prog_track = { card_x + 20, card_y + 80, card_w - 40, 14 };
        draw_rounded_rect(ren, &prog_track, (SDL_Color){ 15, 23, 42, 255 }, 4);

        // Progress fill
        int fill_w = (int)((card_w - 40) * prog);
        if (fill_w > 0) {
            SDL_Rect prog_fill = { card_x + 20, card_y + 80, fill_w, 14 };
            draw_rounded_rect(ren, &prog_fill, logo_col, 4);
        }
    } else if (state == PLAYER_STATE_PAUSED) {
        // Paused watermark
        int icon_size = 60;
        int icon_x = vp_x + (vp_w - icon_size) / 2;
        int icon_y = vp_y + (vp_h - icon_size) / 2;
        draw_text(ren, icon_x, icon_y, "[ PAUSED ]", (SDL_Color){ 255, 255, 255, 180 }, 1.3f);
    } else if (state == PLAYER_STATE_IDLE && !arch) {
        // Welcome empty state
        int msg_x = vp_x + (vp_w - 340) / 2;
        int msg_y = vp_y + (vp_h - 100) / 2;
        draw_text(ren, msg_x, msg_y, "DIRECTZIP VIDEO ENGINE", logo_col, 1.25f);
        draw_text(ren, msg_x, msg_y + 35, "Drag & Drop a ZIP archive here", white_col, 1.0f);
        draw_text(ren, msg_x, msg_y + 60, "or click [Open ZIP] to start watching.", gray_col, 0.85f);
        draw_text(ren, msg_x, msg_y + 85, "* Videos play directly from RAM (Zero disk writes)", (SDL_Color){ 16, 185, 129, 255 }, 0.8f);
    } else if (state == PLAYER_STATE_ERROR) {
        // Error banner
        int err_x = vp_x + (vp_w - 420) / 2;
        int err_y = vp_y + (vp_h - 90) / 2;
        SDL_Rect err_box = { err_x, err_y, 420, 90 };
        draw_rounded_rect(ren, &err_box, (SDL_Color){ 69, 10, 10, 240 }, 6);
        draw_text(ren, err_x + 20, err_y + 16, "PLAYBACK ERROR", (SDL_Color){ 248, 113, 113, 255 }, 1.0f);
        draw_text(ren, err_x + 20, err_y + 44, player_controller_get_status_text(pc), (SDL_Color){ 254, 202, 202, 255 }, 0.85f);
    }

    // ==========================================
    // 4. BOTTOM PLAYER CONTROLS
    // ==========================================
    int bot_y = ui->win_height - ui->bottombar_height;
    SDL_Rect botbar = { 0, bot_y, ui->win_width, ui->bottombar_height };
    SDL_Color botbar_col = { 23, 25, 35, 255 };
    draw_rounded_rect(ren, &botbar, botbar_col, 0);

    // Top border of bottom bar
    SDL_SetRenderDrawColor(ren, 38, 42, 60, 255);
    SDL_RenderDrawLine(ren, 0, bot_y, ui->win_width, bot_y);

    // Timeline Progress Bar
    int tl_x = ui->sidebar_width + 120;
    int tl_y = bot_y + 12;
    int tl_w = ui->win_width - ui->sidebar_width - 240;
    int tl_h = 6;

    double cur_t = player_controller_get_current_time(pc);
    double dur_t = player_controller_get_duration(pc);
    double prog_frac = (dur_t > 0.0) ? (cur_t / dur_t) : 0.0;
    if (prog_frac < 0.0) prog_frac = 0.0;
    if (prog_frac > 1.0) prog_frac = 1.0;

    // Track
    SDL_Rect tl_bg = { tl_x, tl_y, tl_w, tl_h };
    draw_rounded_rect(ren, &tl_bg, (SDL_Color){ 51, 65, 85, 255 }, 3);

    // Filled progress
    int fill_tl_w = (int)(tl_w * prog_frac);
    if (fill_tl_w > 0) {
        SDL_Rect tl_fill = { tl_x, tl_y, fill_tl_w, tl_h };
        draw_rounded_rect(ren, &tl_fill, logo_col, 3);
    }

    // Scrubber thumb
    int thumb_x = tl_x + fill_tl_w;
    SDL_Rect thumb = { thumb_x - 5, tl_y - 4, 10, 14 };
    draw_rounded_rect(ren, &thumb, white_col, 3);

    // Time indicators: Left = current time, Right = duration
    char cur_t_str[16], dur_t_str[16];
    format_time_str(cur_t, cur_t_str, sizeof(cur_t_str));
    format_time_str(dur_t, dur_t_str, sizeof(dur_t_str));

    draw_text(ren, tl_x - 65, tl_y - 4, cur_t_str, gray_col, 0.85f);
    draw_text(ren, tl_x + tl_w + 14, tl_y - 4, dur_t_str, gray_col, 0.85f);

    // Bottom Action Buttons
    int ctrl_y = bot_y + 36;
    int play_btn_x = ui->sidebar_width + 70;

    // Prev Button
    bool prev_hov = (ui->mouse_x >= play_btn_x - 45 && ui->mouse_x <= play_btn_x - 10 && ui->mouse_y >= ctrl_y && ui->mouse_y <= ctrl_y + 32);
    SDL_Rect prev_btn = { play_btn_x - 45, ctrl_y, 35, 32 };
    draw_rounded_rect(ren, &prev_btn, prev_hov ? (SDL_Color){ 51, 65, 85, 255 } : (SDL_Color){ 30, 41, 59, 255 }, 4);
    draw_text(ren, play_btn_x - 36, ctrl_y + 8, "|<", white_col, 1.0f);

    // Play/Pause Button
    bool play_hov = (ui->mouse_x >= play_btn_x && ui->mouse_x <= play_btn_x + 36 && ui->mouse_y >= ctrl_y && ui->mouse_y <= ctrl_y + 32);
    SDL_Rect play_btn = { play_btn_x, ctrl_y, 36, 32 };
    draw_rounded_rect(ren, &play_btn, play_hov ? (SDL_Color){ 37, 99, 235, 255 } : logo_col, 4);
    if (state == PLAYER_STATE_PLAYING) {
        draw_text(ren, play_btn_x + 10, ctrl_y + 8, "||", white_col, 1.0f);
    } else {
        draw_text(ren, play_btn_x + 12, ctrl_y + 8, ">", white_col, 1.0f);
    }

    // Next Button
    bool next_hov = (ui->mouse_x >= play_btn_x + 45 && ui->mouse_x <= play_btn_x + 80 && ui->mouse_y >= ctrl_y && ui->mouse_y <= ctrl_y + 32);
    SDL_Rect next_btn = { play_btn_x + 45, ctrl_y, 35, 32 };
    draw_rounded_rect(ren, &next_btn, next_hov ? (SDL_Color){ 51, 65, 85, 255 } : (SDL_Color){ 30, 41, 59, 255 }, 4);
    draw_text(ren, play_btn_x + 54, ctrl_y + 8, ">|", white_col, 1.0f);

    // Seek -10s
    bool s_back_hov = (ui->mouse_x >= play_btn_x + 90 && ui->mouse_x <= play_btn_x + 130 && ui->mouse_y >= ctrl_y && ui->mouse_y <= ctrl_y + 32);
    SDL_Rect s_back_btn = { play_btn_x + 90, ctrl_y, 40, 32 };
    draw_rounded_rect(ren, &s_back_btn, s_back_hov ? (SDL_Color){ 51, 65, 85, 255 } : (SDL_Color){ 30, 41, 59, 255 }, 4);
    draw_text(ren, play_btn_x + 95, ctrl_y + 8, "-10", gray_col, 0.85f);

    // Seek +10s
    bool s_fwd_hov = (ui->mouse_x >= play_btn_x + 135 && ui->mouse_x <= play_btn_x + 175 && ui->mouse_y >= ctrl_y && ui->mouse_y <= ctrl_y + 32);
    SDL_Rect s_fwd_btn = { play_btn_x + 135, ctrl_y, 40, 32 };
    draw_rounded_rect(ren, &s_fwd_btn, s_fwd_hov ? (SDL_Color){ 51, 65, 85, 255 } : (SDL_Color){ 30, 41, 59, 255 }, 4);
    draw_text(ren, play_btn_x + 140, ctrl_y + 8, "+10", gray_col, 0.85f);

    // Volume Slider & Mute Toggle
    int vol_btn_x = ui->win_width - 190;
    bool is_mut = player_controller_is_muted(pc);
    float vol = player_controller_get_volume(pc);

    draw_text(ren, vol_btn_x, ctrl_y + 8, is_mut ? "MUT" : "VOL", is_mut ? (SDL_Color){ 239, 68, 68, 255 } : gray_col, 0.85f);

    int vol_bar_x = ui->win_width - 150;
    int vol_bar_w = 70;
    SDL_Rect vol_bg = { vol_bar_x, ctrl_y + 12, vol_bar_w, 6 };
    draw_rounded_rect(ren, &vol_bg, (SDL_Color){ 51, 65, 85, 255 }, 3);

    int vol_fill_w = is_mut ? 0 : (int)(vol_bar_w * vol);
    if (vol_fill_w > 0) {
        SDL_Rect vol_fill = { vol_bar_x, ctrl_y + 12, vol_fill_w, 6 };
        draw_rounded_rect(ren, &vol_fill, logo_col, 3);
    }

    // Fullscreen Toggle Button
    bool fs_hov = (ui->mouse_x >= ui->win_width - 65 && ui->mouse_x <= ui->win_width - 20 && ui->mouse_y >= ctrl_y && ui->mouse_y <= ctrl_y + 32);
    SDL_Rect fs_btn = { ui->win_width - 65, ctrl_y, 45, 32 };
    draw_rounded_rect(ren, &fs_btn, fs_hov ? (SDL_Color){ 51, 65, 85, 255 } : (SDL_Color){ 30, 41, 59, 255 }, 4);
    draw_text(ren, ui->win_width - 55, ctrl_y + 8, "[ ]", white_col, 1.0f);

    // ==========================================
    // 5. VIDEO QUALITY & 1080P UPSCALING MODAL
    // ==========================================
    if (ui->show_quality_menu) {
        int mw = 520, mh = 380;
        int qx = (ui->win_width - mw) / 2;
        int qy = (ui->win_height - mh) / 2;

        // Dark modal backdrop
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 10, 15, 26, 180);
        SDL_Rect backdrop = { 0, 0, ui->win_width, ui->win_height };
        SDL_RenderFillRect(ren, &backdrop);

        // Modal Card Window
        SDL_Rect modal_box = { qx, qy, mw, mh };
        draw_rounded_rect(ren, &modal_box, (SDL_Color){ 23, 28, 42, 255 }, 8);

        // Border
        SDL_SetRenderDrawColor(ren, 59, 130, 246, 255);
        SDL_RenderDrawRect(ren, &modal_box);

        // Header Title
        draw_text(ren, qx + 20, qy + 16, "VIDEO QUALITY & 1080P UPSCALER", (SDL_Color){ 147, 197, 253, 255 }, 1.05f);

        // Close button [X]
        bool close_hov = (ui->mouse_x >= qx + mw - 40 && ui->mouse_x <= qx + mw - 10 && ui->mouse_y >= qy + 10 && ui->mouse_y <= qy + 36);
        SDL_Rect close_btn = { qx + mw - 40, qy + 10, 30, 26 };
        draw_rounded_rect(ren, &close_btn, close_hov ? (SDL_Color){ 239, 68, 68, 255 } : (SDL_Color){ 51, 65, 85, 255 }, 4);
        draw_text(ren, qx + mw - 30, qy + 15, "X", white_col, 0.95f);

        // Divider
        SDL_SetRenderDrawColor(ren, 38, 42, 60, 255);
        SDL_RenderDrawLine(ren, qx + 16, qy + 44, qx + mw - 16, qy + 44);

        // SECTION 1: RESOLUTION / UPSCALING MODE
        draw_text(ren, qx + 20, qy + 48, "RESOLUTION MODE (Press 'U' to cycle):", gray_col, 0.85f);

        UpscalingMode cur_mode = player_controller_get_upscaling_mode(pc);
        UpscalingQuality cur_qual = player_controller_get_upscaling_quality(pc);

        // Option 1: Automatic 1080p
        bool m1_sel = (cur_mode == UPSCALING_MODE_AUTO_1080P);
        bool m1_hov = (ui->mouse_x >= qx + 20 && ui->mouse_x <= qx + mw - 20 && ui->mouse_y >= qy + 65 && ui->mouse_y <= qy + 90);
        SDL_Rect m1_rect = { qx + 20, qy + 65, mw - 40, 26 };
        draw_rounded_rect(ren, &m1_rect, m1_sel ? (SDL_Color){ 30, 58, 138, 255 } : m1_hov ? (SDL_Color){ 30, 41, 59, 255 } : (SDL_Color){ 19, 23, 34, 255 }, 4);
        draw_text(ren, qx + 30, qy + 70, m1_sel ? "[*] Automatic 1080p (Upscale sub-1080p, preserve >=1080p)" : "[ ] Automatic 1080p (Upscale sub-1080p, preserve >=1080p)", m1_sel ? (SDL_Color){ 96, 165, 250, 255 } : white_col, 0.88f);

        // Option 2: Original Resolution
        bool m2_sel = (cur_mode == UPSCALING_MODE_ORIGINAL);
        bool m2_hov = (ui->mouse_x >= qx + 20 && ui->mouse_x <= qx + mw - 20 && ui->mouse_y >= qy + 95 && ui->mouse_y <= qy + 120);
        SDL_Rect m2_rect = { qx + 20, qy + 95, mw - 40, 26 };
        draw_rounded_rect(ren, &m2_rect, m2_sel ? (SDL_Color){ 30, 58, 138, 255 } : m2_hov ? (SDL_Color){ 30, 41, 59, 255 } : (SDL_Color){ 19, 23, 34, 255 }, 4);
        draw_text(ren, qx + 30, qy + 100, m2_sel ? "[*] Original Resolution (Native 1:1, No Scaling)" : "[ ] Original Resolution (Native 1:1, No Scaling)", m2_sel ? (SDL_Color){ 96, 165, 250, 255 } : white_col, 0.88f);

        // Option 3: Automatic Best Fit
        bool m3_sel = (cur_mode == UPSCALING_MODE_BEST_FIT);
        bool m3_hov = (ui->mouse_x >= qx + 20 && ui->mouse_x <= qx + mw - 20 && ui->mouse_y >= qy + 125 && ui->mouse_y <= qy + 150);
        SDL_Rect m3_rect = { qx + 20, qy + 125, mw - 40, 26 };
        draw_rounded_rect(ren, &m3_rect, m3_sel ? (SDL_Color){ 30, 58, 138, 255 } : m3_hov ? (SDL_Color){ 30, 41, 59, 255 } : (SDL_Color){ 19, 23, 34, 255 }, 4);
        draw_text(ren, qx + 30, qy + 130, m3_sel ? "[*] Automatic Best Fit (Scale to fill current player window)" : "[ ] Automatic Best Fit (Scale to fill current player window)", m3_sel ? (SDL_Color){ 96, 165, 250, 255 } : white_col, 0.88f);

        // SECTION 2: SCALING ALGORITHM
        draw_text(ren, qx + 20, qy + 162, "SCALING ALGORITHM (Press 'K' to cycle):", gray_col, 0.85f);

        // High Quality (Lanczos)
        bool q1_sel = (cur_qual == UPSCALING_QUALITY_HIGH);
        bool q1_hov = (ui->mouse_x >= qx + 20 && ui->mouse_x <= qx + 170 && ui->mouse_y >= qy + 185 && ui->mouse_y <= qy + 215);
        SDL_Rect q1_rect = { qx + 20, qy + 185, 150, 30 };
        draw_rounded_rect(ren, &q1_rect, q1_sel ? (SDL_Color){ 16, 185, 129, 255 } : q1_hov ? (SDL_Color){ 51, 65, 85, 255 } : (SDL_Color){ 30, 41, 59, 255 }, 4);
        draw_text(ren, qx + 30, qy + 192, q1_sel ? "* Lanczos (High)" : "  Lanczos (High)", white_col, 0.85f);

        // Balanced (Bicubic)
        bool q2_sel = (cur_qual == UPSCALING_QUALITY_BALANCED);
        bool q2_hov = (ui->mouse_x >= qx + 180 && ui->mouse_x <= qx + 330 && ui->mouse_y >= qy + 185 && ui->mouse_y <= qy + 215);
        SDL_Rect q2_rect = { qx + 180, qy + 185, 150, 30 };
        draw_rounded_rect(ren, &q2_rect, q2_sel ? (SDL_Color){ 16, 185, 129, 255 } : q2_hov ? (SDL_Color){ 51, 65, 85, 255 } : (SDL_Color){ 30, 41, 59, 255 }, 4);
        draw_text(ren, qx + 190, qy + 192, q2_sel ? "* Bicubic (Mid)" : "  Bicubic (Mid)", white_col, 0.85f);

        // Performance (Bilinear)
        bool q3_sel = (cur_qual == UPSCALING_QUALITY_PERFORMANCE);
        bool q3_hov = (ui->mouse_x >= qx + 340 && ui->mouse_x <= qx + 490 && ui->mouse_y >= qy + 185 && ui->mouse_y <= qy + 215);
        SDL_Rect q3_rect = { qx + 340, qy + 185, 150, 30 };
        draw_rounded_rect(ren, &q3_rect, q3_sel ? (SDL_Color){ 16, 185, 129, 255 } : q3_hov ? (SDL_Color){ 51, 65, 85, 255 } : (SDL_Color){ 30, 41, 59, 255 }, 4);
        draw_text(ren, qx + 348, qy + 192, q3_sel ? "* Bilinear (Fast)" : "  Bilinear (Fast)", white_col, 0.85f);

        // SECTION 3: REAL-TIME TELEMETRY
        SDL_SetRenderDrawColor(ren, 38, 42, 60, 255);
        SDL_RenderDrawLine(ren, qx + 16, qy + 228, qx + mw - 16, qy + 228);

        VideoUpscalerInfo inf = player_controller_get_upscaler_info(pc);

        char tbuf1[96], tbuf2[96], tbuf3[96];
        if (inf.original_width > 0) {
            snprintf(tbuf1, sizeof(tbuf1), "Original: %dx%d (%.2f:1) -> Output: %dx%d",
                     inf.original_width, inf.original_height,
                     inf.original_height > 0 ? (double)inf.original_width / inf.original_height : 0.0,
                     inf.output_width, inf.output_height);
            snprintf(tbuf2, sizeof(tbuf2), "Status  : %s (Factor: %.2fx) | Pipeline: %s",
                     inf.is_active ? "UPSCALING ACTIVE" : "NATIVE PASS-THROUGH",
                     inf.scale_factor, inf.algorithm_name);
        } else {
            snprintf(tbuf1, sizeof(tbuf1), "Original: No video currently loaded");
            snprintf(tbuf2, sizeof(tbuf2), "Status  : Standby | Engine ready for in-memory stream");
        }
        snprintf(tbuf3, sizeof(tbuf3), "Shortcuts: [Q] Toggle Menu | [U] Cycle Mode | [K] Cycle Quality");

        draw_text(ren, qx + 20, qy + 238, tbuf1, (SDL_Color){ 241, 245, 249, 255 }, 0.85f);
        draw_text(ren, qx + 20, qy + 258, tbuf2, inf.is_active ? (SDL_Color){ 52, 211, 153, 255 } : (SDL_Color){ 148, 163, 184, 255 }, 0.85f);
        draw_text(ren, qx + 20, qy + 280, tbuf3, (SDL_Color){ 148, 163, 184, 255 }, 0.82f);

        // SECTION 4: CLOSE / APPLY BUTTON
        bool apply_hov = (ui->mouse_x >= qx + (mw - 140) / 2 && ui->mouse_x <= qx + (mw + 140) / 2 && ui->mouse_y >= qy + mh - 42 && ui->mouse_y <= qy + mh - 12);
        SDL_Rect apply_btn = { qx + (mw - 140) / 2, qy + mh - 42, 140, 30 };
        draw_rounded_rect(ren, &apply_btn, apply_hov ? (SDL_Color){ 37, 99, 235, 255 } : (SDL_Color){ 30, 58, 138, 255 }, 4);
        draw_text(ren, qx + (mw - 60) / 2, qy + mh - 35, "Done", white_col, 0.95f);
    }

    SDL_RenderPresent(ren);
}
