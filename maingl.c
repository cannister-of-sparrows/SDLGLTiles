/*
 * This file is part of the Tilemap SDL/OpenGL tech demo.
 *
 * Copyright (C) 2025 Cannister of Sparrows <cannister_of_sparrows@proton.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

// Compile with: gcc maingl.c -o tilemap_demo_opengl -lSDL2 -lSDL2_image -lGL -lm -std=c99

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

// --- Configuration constants ---
#define SCREEN_WIDTH 800                  // Initial window width
#define SCREEN_HEIGHT 600                 // Initial window height
#define MAP_WIDTH 1000                    // Tile map width in tiles
#define MAP_HEIGHT 1000                   // Tile map height in tiles
#define TILE_WIDTH 32                     // Width of each tile in pixels
#define TILE_HEIGHT 32                    // Height of each tile in pixels
#define LOD_PIXEL_THRESHOLD 8.0f          // Threshold below which LOD kicks in
#define MAX_ZOOM 16.0f                    // Maximum zoom level
#define MIN_ZOOM 0.001f                   // Minimum zoom level
#define ZOOM_STEP 1.1f                    // Zoom in/out factor
#define OUTLINE_PIXEL_WIDTH 8.0f          // Width of the outline in pixels

// --- Tile asset metadata and OpenGL texture handle ---
typedef struct {
    char* filepath;
    int tile_width;
    int tile_height;
    int rows;
    int cols;
    GLuint texture_id;

    // Bit shift optimization fields
    int use_shift;      // Whether cols is power of 2
    int shift_bits;     // log2(cols), used for fast division
} Tileset;

// --- Map storage using a flat array for performance ---
typedef struct {
    int* tiles;
} TileMap;

// --- Helper: check if integer is power of two ---
int is_power_of_two(int x) {
    return x > 0 && (x & (x - 1)) == 0;
}

// --- Load tileset texture and calculate tile grid ---
int load_tileset(Tileset* tileset) {
    SDL_Surface* surface = IMG_Load(tileset->filepath);
    if (!surface) {
        printf("IMG_Load failed: %s\n", IMG_GetError());
        return 0;
    }

    glGenTextures(1, &tileset->texture_id);
    glBindTexture(GL_TEXTURE_2D, tileset->texture_id);

    // Use nearest filtering to prevent bleeding artifacts
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    GLint format = surface->format->BytesPerPixel == 4 ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, surface->w, surface->h, 0, format, GL_UNSIGNED_BYTE, surface->pixels);

    tileset->cols = surface->w / tileset->tile_width;
    tileset->rows = surface->h / tileset->tile_height;

    // Detect and record if we can use bit shift optimization
    tileset->use_shift = is_power_of_two(tileset->cols);
    if (tileset->use_shift) {
        tileset->shift_bits = (int)log2f((float)tileset->cols);
    }

    SDL_FreeSurface(surface);
    return 1;
}

// --- Fill tilemap with random tile indices ---
void fill_random_tilemap(TileMap* map, int max_tile_index) {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            map->tiles[y * MAP_WIDTH + x] = rand() % max_tile_index;
        }
    }
}

// --- Optimized draw_tile with shift logic and precomputed UV steps ---
void draw_tile(
    int tx, int ty, int tile_index,
    Tileset* tileset,
    float zoom, float offset_x, float offset_y,
    int lod,
    float step_u, float step_v) { // Precomputed 1/cols and 1/rows

    int tw = tileset->tile_width;
    int th = tileset->tile_height;

    int sx, sy;
    if (tileset->use_shift) {
        sx = tile_index & (tileset->cols - 1);      // tile_index % cols (fast)
        sy = tile_index >> tileset->shift_bits;     // tile_index / cols (fast)
    } else {
        sx = tile_index % tileset->cols;
        sy = tile_index / tileset->cols;
    }

    // Use precomputed texture step size
    float u  = sx * step_u;
    float v  = sy * step_v;
    float u2 = u + step_u;
    float v2 = v + step_v;

    // Convert to screen-space coordinates
    float x = (tx * tw + offset_x) * zoom;
    float y = (ty * th + offset_y) * zoom;
    float w = tw * zoom * lod;
    float h = th * zoom * lod;

    glBegin(GL_QUADS);
    glTexCoord2f(u,  v);   glVertex2f(x, y);
    glTexCoord2f(u2, v);   glVertex2f(x + w, y);
    glTexCoord2f(u2, v2);  glVertex2f(x + w, y + h);
    glTexCoord2f(u,  v2);  glVertex2f(x, y + h);
    glEnd();
}

// --- Draw a red outline box around hovered tile ---
void draw_tile_outline(int tile_x, int tile_y, float zoom, float offset_x, float offset_y) {
    float x = (tile_x * TILE_WIDTH + offset_x) * zoom;
    float y = (tile_y * TILE_HEIGHT + offset_y) * zoom;
    float w = TILE_WIDTH * zoom;
    float h = TILE_HEIGHT * zoom;
    float px = OUTLINE_PIXEL_WIDTH;

    glDisable(GL_TEXTURE_2D);
    glColor3f(1.0f, 0.0f, 0.0f); // Red

    // Four edges of the box
    glBegin(GL_QUADS); // Top
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + px); glVertex2f(x, y + px);
    glEnd();

    glBegin(GL_QUADS); // Bottom
    glVertex2f(x, y + h - px); glVertex2f(x + w, y + h - px); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();

    glBegin(GL_QUADS); // Left
    glVertex2f(x, y); glVertex2f(x + px, y); glVertex2f(x + px, y + h); glVertex2f(x, y + h);
    glEnd();

    glBegin(GL_QUADS); // Right
    glVertex2f(x + w - px, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x + w - px, y + h);
    glEnd();

    glEnable(GL_TEXTURE_2D);
    glColor3f(1.0f, 1.0f, 1.0f);
}


int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    SDL_Window* window = SDL_CreateWindow("Tilemap OpenGL", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);

    Tileset tileset = {"tileset.png", TILE_WIDTH, TILE_HEIGHT, 0, 0, 0};
    if (!load_tileset(&tileset)) return 1;

    TileMap map;
    map.tiles = malloc(sizeof(int) * MAP_WIDTH * MAP_HEIGHT);
    if (!map.tiles) {
        fprintf(stderr, "Failed to allocate memory for tilemap\n");
        return 1;
    }

    // Determine if tileset.cols is a power of two for bit-shift optimization
    tileset.use_shift = (tileset.cols & (tileset.cols - 1)) == 0;
    if (tileset.use_shift) {
        int shift = 0;
        while ((1 << shift) < tileset.cols) shift++;
        tileset.shift_bits = shift;
    }

    float step_u = 1.0f / tileset.cols;
    float step_v = 1.0f / tileset.rows;

    srand((unsigned int)time(NULL));
    fill_random_tilemap(&map, tileset.cols * tileset.rows);

    float offset_x = (MAP_WIDTH * TILE_WIDTH - SCREEN_WIDTH) / -2.0f;
    float offset_y = (MAP_HEIGHT * TILE_HEIGHT - SCREEN_HEIGHT) / -2.0f;
    float zoom = 1.0f;

    int dragging = 0;
    int last_mouse_x = 0, last_mouse_y = 0;

    Uint32 fps_last_time = SDL_GetTicks();
    int fps_frames = 0;

    int running = 1;
    SDL_Event e;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                dragging = 1;
                last_mouse_x = e.button.x;
                last_mouse_y = e.button.y;
            } else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                dragging = 0;
            } else if (e.type == SDL_MOUSEMOTION && dragging) {
                offset_x += (e.motion.x - last_mouse_x) / zoom;
                offset_y += (e.motion.y - last_mouse_y) / zoom;
                last_mouse_x = e.motion.x;
                last_mouse_y = e.motion.y;
            } else if (e.type == SDL_MOUSEWHEEL) {
                int mx, my;
                SDL_GetMouseState(&mx, &my);
                float world_x = mx / zoom - offset_x;
                float world_y = my / zoom - offset_y;
                zoom *= (e.wheel.y > 0) ? ZOOM_STEP : (1.0f / ZOOM_STEP);
                if (zoom < MIN_ZOOM) zoom = MIN_ZOOM;
                if (zoom > MAX_ZOOM) zoom = MAX_ZOOM;
                offset_x = mx / zoom - world_x;
                offset_y = my / zoom - world_y;
            } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                int width = e.window.data1;
                int height = e.window.data2;
                glViewport(0, 0, width, height);
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                glOrtho(0, width, height, 0, -1, 1);
                glMatrixMode(GL_MODELVIEW);
            }
        }

        int screen_w, screen_h;
        SDL_GetWindowSize(window, &screen_w, &screen_h); // Get current window size (important if user resized)

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT); // Clear the screen before rendering

        glBindTexture(GL_TEXTURE_2D, tileset.texture_id); // Bind the tileset texture for drawing

        // --- PERFORMANCE OPTIMISATION: Level of Detail (LOD) ---
        // If the size of a tile on screen is smaller than LOD_PIXEL_THRESHOLD,
        // we increase LOD (skip tiles) to reduce draw calls and speed up rendering.
        float tsz = TILE_WIDTH * zoom;
        int lod = (tsz < LOD_PIXEL_THRESHOLD) ? (int)ceilf(LOD_PIXEL_THRESHOLD / tsz) : 1;

        // --- PERFORMANCE OPTIMISATION: View Clipping ---
        // Compute only the visible tile bounds to avoid drawing offscreen tiles.
        int min_x = (int)floorf(-offset_x / TILE_WIDTH);
        int min_y = (int)floorf(-offset_y / TILE_HEIGHT);
        int max_x = (int)ceilf((screen_w / zoom - offset_x) / TILE_WIDTH);
        int max_y = (int)ceilf((screen_h / zoom - offset_y) / TILE_HEIGHT);

        // Clamp bounds to valid map size
        if (min_x < 0) min_x = 0;
        if (min_y < 0) min_y = 0;
        if (max_x > MAP_WIDTH) max_x = MAP_WIDTH;
        if (max_y > MAP_HEIGHT) max_y = MAP_HEIGHT;

        // Align LOD grouping so same tiles are chosen as we pan
        int start_x = (min_x / lod) * lod;
        int start_y = (min_y / lod) * lod;

        // --- DRAW TILES ---
        for (int y = start_y; y < max_y; y += lod) {
            for (int x = start_x; x < max_x; x += lod) {
                int tile_index = map.tiles[y * MAP_WIDTH + x];

                // Call draw_tile with precomputed texture steps and shift-optimized indexing
                draw_tile(x, y, tile_index, &tileset, zoom, offset_x, offset_y, lod, step_u, step_v);
            }
        }

        // --- MOUSE HOVER TILE OUTLINE ---
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        int tile_x = (int)((mx / zoom - offset_x) / TILE_WIDTH);
        int tile_y = (int)((my / zoom - offset_y) / TILE_HEIGHT);

        if (tile_x >= 0 && tile_x < MAP_WIDTH && tile_y >= 0 && tile_y < MAP_HEIGHT) {
            draw_tile_outline(tile_x, tile_y, zoom, offset_x, offset_y); // Draw red 1px outline around hovered tile
        }

        SDL_GL_SwapWindow(window); // Present the rendered frame

        // --- FPS COUNTER ---
        fps_frames++;
        Uint32 fps_current_time = SDL_GetTicks();
        if (fps_current_time > fps_last_time + 1000) {
            float fps = fps_frames * 1000.0f / (fps_current_time - fps_last_time);
            char title[128];
            snprintf(title, sizeof(title), "Tilemap OpenGL - FPS: %.2f | Zoom: %.2f | LOD: %d", fps, zoom, lod);
            SDL_SetWindowTitle(window, title); // Display FPS and zoom level in the title bar

            fps_last_time = fps_current_time;
            fps_frames = 0;
        }

        SDL_Delay(16); // Optional cap to ~60 FPS
    }

    free(map.tiles);
    glDeleteTextures(1, &tileset.texture_id);
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}

