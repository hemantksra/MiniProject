#ifndef EDITOR_H
#define EDITOR_H

#include <ncurses/ncurses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
typedef void* HANDLE;
typedef unsigned long DWORD;
#define STD_INPUT_HANDLE ((DWORD)-10)
#define ENABLE_QUICK_EDIT_MODE 0x0040
#define ENABLE_EXTENDED_FLAGS 0x0080
#define ENABLE_MOUSE_INPUT 0x0010

void* __stdcall GetStdHandle(DWORD nStdHandle);
int __stdcall GetConsoleMode(void* hConsoleHandle, DWORD *lpMode);
int __stdcall SetConsoleMode(void* hConsoleHandle, DWORD dwMode);
#endif

#define WIDTH 80
#define HEIGHT 15
#define MAX_SHAPES 100
#define BG_CHAR '_'
#define FG_CHAR '*'

typedef enum {
  SHAPE_LINE,
  SHAPE_RECTANGLE,
  SHAPE_CIRCLE,
  SHAPE_TRIANGLE
} ShapeType;

typedef struct {
  int id;
  ShapeType type;
  int color_pair;
  bool is_filled;
  union {
    struct {
      int x1, y1, x2, y2;
    } line;
    struct {
      int x, y, width, height;
    } rect;
    struct {
      int cx, cy, r;
    } circle;
    struct {
      int x1, y1, x2, y2, x3, y3;
    } triangle;
  } data;
} Shape;

typedef enum {
  MODE_MENU_MAIN,
  MODE_MENU_ADD,
  MODE_MENU_LIST,
  MODE_MENU_MODIFY,
  MODE_MENU_DELETE,
  MODE_INTERACTIVE_PLACE,
  MODE_INTERACTIVE_MODIFY
} EditorMode;

extern Shape shapes[MAX_SHAPES];
extern int shape_count;
extern int next_id;
extern chtype canvas[HEIGHT][WIDTH];
extern int active_color;
extern bool active_fill;
extern chtype current_render_chtype;

#define MAX_UNDO 50
extern Shape undo_stack[MAX_UNDO][MAX_SHAPES];
extern int undo_shape_counts[MAX_UNDO];
extern int undo_current;
extern int undo_head;

extern float zoom;
extern int pan_x;
extern int pan_y;

extern EditorMode current_mode;
extern int menu_selection;
extern int menu_scroll;

extern int cursor_x;
extern int cursor_y;

extern ShapeType place_shape_type;
extern int place_points_collected;
extern int place_px[3];
extern int place_py[3];

extern bool blink_state;
extern bool render_direct_mode;
extern int render_direct_attr;

// state.c
void save_state();
void do_undo();
void do_redo();
void finalize_placement();
void translate_shape(Shape *s, int dx, int dy);
void scale_shape(Shape *s, int delta);

// graphics.c
int screen_to_world_x(int sx);
int screen_to_world_y(int sy);
void clear_canvas();
void print_canvas();
void draw_pixel(int x, int y);
void draw_line(int x1, int y1, int x2, int y2);
void draw_rect(int x, int y, int w, int h);
void draw_circle(int cx, int cy, int radius);
void draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3);
void draw_filled_rect(int x, int y, int w, int h);
void draw_filled_circle(int cx, int cy, int radius);
int point_in_triangle(int px, int py, int x1, int y1, int x2, int y2, int x3, int y3);
void draw_filled_triangle(int x1, int y1, int x2, int y2, int x3, int y3);
void render_single_shape(Shape *s);
void render_shapes();

// ui.c
void print_menu_item(int row, const char *text, bool is_selected);
void draw_ui();

#endif
