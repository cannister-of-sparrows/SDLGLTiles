# Tilemap Viewer (SDL2 + OpenGL)

This repository contains two implementations of a 2D tilemap renderer:

- **main.c** – Uses SDL2's built-in rendering API
- **maingl.c** – Uses OpenGL 1.1 directly for rendering

Both demonstrate high-performance rendering of large tilemaps with optimisations for smooth panning, zooming, and mouse interaction.

---

## Features (Both Versions)

- **View panning** with mouse drag
- **Zooming** with mouse scroll, centred on cursor
- **Tile highlighting** under mouse cursor with a pixel-perfect outline
- **FPS counter** and zoom/LOD display in window title
- **Hardware-accelerated rendering** (both SDL2 and OpenGL)
- **Efficient memory layout** using a flat tile array

---

## Optimisations

Both `main.c` and `maingl.c` implement:

- **View Clipping**: Only visible tiles are rendered
- **LOD Skipping**: When tiles appear too small, rendering is skipped for many of them
- **Group Rendering**: Tiles at low zoom are grouped and expanded to avoid overdraw

The `maingl.c` version adds further optimisations:

- **Bitwise shift and mask** optimisations when tile sizes and tile counts are powers of two (detected at runtime)
- **Full control of rasterisation** to eliminate texture bleeding and black lines at tile edges (a limitation in `main.c`)

---

## Limitations

- Requires a `tileset.png` file (not included)
- Assumes all tiles in the tileset are laid out in a regular grid
- Uses OpenGL 1.1 for compatibility; no shaders or modern features

---

## License

This project is licensed under the **GNU GPL v3**. See the [LICENSE](./LICENSE) file for details.
