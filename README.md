# Terminal Vector Graphics Editor

A lightweight, powerful, terminal-based vector graphics editor written entirely in C. It uses `ncurses` to render shapes directly in your console, supporting mouse interactions, zooming, panning, and a full undo/redo stack.

## Features

- **Shapes**: Draw Lines, Rectangles, Circles, and Triangles.
- **Attributes**: Toggle between 7 ANSI colors and toggle shape filling (solid vs. outline).
- **Interactive Placement**: Real-time shape previews while placing points with the mouse or keyboard.
- **Modify & Delete**: Interactively move (translate), resize (scale), recolor, or delete existing shapes.
- **Camera Controls**: Infinite canvas with Panning (W/A/S/D) and Zooming (+/-).
- **History**: Full Undo (U) and Redo (R) history stack supporting up to 50 operations.
- **Input**: Full mouse support for clicking menus and placing shapes, alongside full keyboard navigation.

## Architecture

The codebase is structured in a modular, MVC-like pattern to ensure it remains clean and easy to expand:

- `editor.h`: Core definitions, structures, and function prototypes.
- `main.c` (Controller): The primary event loop, input routing, and ncurses initialization.
- `state.c` (Model): Manages the shape array, undo/redo stacks, and math transformations (scale/translate).
- `graphics.c` (View - Canvas): Implements Bresenham's algorithms to rasterize mathematical shapes into the terminal grid.
- `ui.c` (View - Interface): Renders the borders, menus, and text overlays.

## Building and Running

### Prerequisites
- GCC Compiler
- `ncursesw` library (Ensure you have a version compiled with wide-character and mouse support).
- Windows users must run this in a terminal that supports ANSI/ncurses (e.g., Windows Terminal).

### Compilation
A `build.bat` script is provided for Windows users:
```bat
.\build.bat
```
Alternatively, compile manually using GCC:
```bash
gcc *.c -o editor.exe -lncursesw
```

### Execution
Run the compiled executable:
```bash
.\editor.exe
```

## Controls

### Global Hotkeys
- `W / A / S / D` : Pan the camera
- `+ / =` : Zoom In
- `-` : Zoom Out
- `U` : Undo
- `R` : Redo
- `C` : Cycle active color
- `F` : Toggle active fill mode (Solid/Outline)

### Menu Navigation
- `Up / Down Arrows` : Navigate menus
- `Enter` : Select option
- `Mouse Click` : Select menu option directly

### Placing Shapes
- `Mouse Click` : Place a point
- `Arrow Keys` : Move cursor manually
- `Enter` : Set point manually
- `ESC` : Cancel placement

### Modifying Shapes
- `Arrow Keys` : Move (Translate) the shape
- `+ / -` : Scale (Resize) the shape
- `C` : Change color of the specific shape
- `F` : Toggle fill of the specific shape
- `Enter / ESC` : Confirm modification and return to menu
