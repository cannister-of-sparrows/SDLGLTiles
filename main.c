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

// Compile with: gcc main.c -o tilemap_demo -lSDL2 -lSDL2_image -lm -std=c99

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define MAP_WIDTH 1000
#define MAP_HEIGHT 1000

typedef struct {
    char* filepath;
    int tile_width;
    int tile_height;
    int rows;
    int cols;
    SDL_Texture* texture;
} Tileset;

typedef struct {
    int tiles[MAP_HEIGHT][MAP_WIDTH];
} TileMap;

int load_tileset(SDL_Renderer* renderer, Tileset* tileset) {
    SDL_Surface* surface = IMG_Load(tileset->filepath);
    if (!surface) {
        printf("IMG_Load failed: %s\n", IMG_GetError());
        return 0;
    }
    tileset->texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (!tileset->texture) {
        printf("SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
        return 0;
    }

    return 1;
}

void fill_random_tilemap(TileMap* map, int max_tile_index) {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            map->tiles[y][x] = rand() % max_tile_index;
        }
    }
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0 || IMG_Init(IMG_INIT_PNG) == 0) {
        printf("SDL_Init or IMG_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    SDL_Window* window = SDL_CreateWindow("Tilemap Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    Tileset tileset = {"tileset.png", 32, 32, 0, 0, NULL};
    if (!load_tileset(renderer, &tileset)) return 1;

    int tex_w, tex_h;
    SDL_QueryTexture(tileset.texture, NULL, NULL, &tex_w, &tex_h);
    tileset.cols = tex_w / tileset.tile_width;
    tileset.rows = tex_h / tileset.tile_height;

    TileMap map;
    srand((unsigned int)time(NULL));
    fill_random_tilemap(&map, tileset.cols * tileset.rows);

    float offset_x = (MAP_WIDTH * tileset.tile_width - SCREEN_WIDTH) / -2.0f;
    float offset_y = (MAP_HEIGHT * tileset.tile_height - SCREEN_HEIGHT) / -2.0f;
    int zoom = 1; // Integer zoom only: 1, 2, etc.

    int dragging = 0;
    int last_mouse_x = 0, last_mouse_y = 0;

    Uint32 fps_last_time = SDL_GetTicks();
    int fps_frames = 0;

    SDL_Event e;
    int running = 1;

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
                offset_x += (e.motion.x - last_mouse_x) / (float)zoom;
                offset_y += (e.motion.y - last_mouse_y) / (float)zoom;
                last_mouse_x = e.motion.x;
                last_mouse_y = e.motion.y;
            } else if (e.type == SDL_MOUSEWHEEL) {
                if (e.wheel.y > 0 && zoom < 4) zoom *= 2;
                else if (e.wheel.y < 0 && zoom > 1) zoom /= 2;
            }
        }

        SDL_RenderClear(renderer);

        int tile_screen_w = tileset.tile_width * zoom;
        int tile_screen_h = tileset.tile_height * zoom;

        int min_x = (int)(-offset_x / tileset.tile_width);
        int min_y = (int)(-offset_y / tileset.tile_height);
        int max_x = (int)((SCREEN_WIDTH / (float)zoom - offset_x) / tileset.tile_width) + 1;
        int max_y = (int)((SCREEN_HEIGHT / (float)zoom - offset_y) / tileset.tile_height) + 1;

        if (min_x < 0) min_x = 0;
        if (min_y < 0) min_y = 0;
        if (max_x > MAP_WIDTH) max_x = MAP_WIDTH;
        if (max_y > MAP_HEIGHT) max_y = MAP_HEIGHT;

        for (int y = min_y; y < max_y; y++) {
            for (int x = min_x; x < max_x; x++) {
                int tile_index = map.tiles[y][x];
                int sx = (tile_index % tileset.cols) * tileset.tile_width;
                int sy = (tile_index / tileset.cols) * tileset.tile_height;
                SDL_Rect src = {sx, sy, tileset.tile_width, tileset.tile_height};

                int dx = (x * tileset.tile_width + (int)offset_x) * zoom;
                int dy = (y * tileset.tile_height + (int)offset_y) * zoom;
                SDL_Rect dst = {dx, dy, tile_screen_w, tile_screen_h};

                SDL_RenderCopy(renderer, tileset.texture, &src, &dst);
            }
        }

        int mx, my;
        SDL_GetMouseState(&mx, &my);
        int tile_x = (mx / zoom - (int)offset_x) / tileset.tile_width;
        int tile_y = (my / zoom - (int)offset_y) / tileset.tile_height;

        if (tile_x >= 0 && tile_x < MAP_WIDTH && tile_y >= 0 && tile_y < MAP_HEIGHT) {
            SDL_Rect highlight = {
                (tile_x * tileset.tile_width + (int)offset_x) * zoom,
                (tile_y * tileset.tile_height + (int)offset_y) * zoom,
                tile_screen_w,
                tile_screen_h
            };
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 100);
            SDL_RenderFillRect(renderer, &highlight);
        }

        SDL_RenderPresent(renderer);

        fps_frames++;
        Uint32 fps_current_time = SDL_GetTicks();
        if (fps_current_time > fps_last_time + 1000) {
            float fps = fps_frames * 1000.0f / (fps_current_time - fps_last_time);
            char title[128];
            snprintf(title, sizeof(title), "Tilemap Demo - FPS: %.2f (Zoom: %dx)", fps, zoom);
            SDL_SetWindowTitle(window, title);

            fps_last_time = fps_current_time;
            fps_frames = 0;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_Delay(16);
    }

    SDL_DestroyTexture(tileset.texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}

