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

Shape shapes[MAX_SHAPES];
int shape_count = 0;
int next_id = 1;
chtype canvas[HEIGHT][WIDTH];
int active_color = 1;
bool active_fill = false;
chtype current_render_chtype = 0;

#define MAX_UNDO 50
Shape undo_stack[MAX_UNDO][MAX_SHAPES];
int undo_shape_counts[MAX_UNDO];
int undo_current = -1;
int undo_head = -1;

float zoom = 1.0f;
int pan_x = 0;
int pan_y = 0;

void save_state() {
  if (undo_current < MAX_UNDO - 1) {
    undo_current++;
  } else {
    for (int i = 0; i < MAX_UNDO - 1; i++) {
      undo_shape_counts[i] = undo_shape_counts[i + 1];
      for (int j = 0; j < undo_shape_counts[i]; j++) {
        undo_stack[i][j] = undo_stack[i + 1][j];
      }
    }
  }
  undo_head = undo_current;
  undo_shape_counts[undo_current] = shape_count;
  for (int i = 0; i < shape_count; i++) {
    undo_stack[undo_current][i] = shapes[i];
  }
}

void do_undo() {
  if (undo_current > 0) {
    undo_current--;
    shape_count = undo_shape_counts[undo_current];
    for (int i = 0; i < shape_count; i++) {
      shapes[i] = undo_stack[undo_current][i];
    }
  }
}

void do_redo() {
  if (undo_current < undo_head) {
    undo_current++;
    shape_count = undo_shape_counts[undo_current];
    for (int i = 0; i < shape_count; i++) {
      shapes[i] = undo_stack[undo_current][i];
    }
  }
}

int screen_to_world_x(int sx) { return (int)(sx / zoom) + pan_x; }
int screen_to_world_y(int sy) { return (int)(sy / zoom) + pan_y; }

typedef enum {
  MODE_MENU_MAIN,
  MODE_MENU_ADD,
  MODE_MENU_LIST,
  MODE_MENU_MODIFY,
  MODE_MENU_DELETE,
  MODE_INTERACTIVE_PLACE,
  MODE_INTERACTIVE_MODIFY
} EditorMode;

EditorMode current_mode = MODE_MENU_MAIN;
int menu_selection = 0;
int menu_scroll = 0;

int cursor_x = WIDTH / 2;
int cursor_y = HEIGHT / 2;

ShapeType place_shape_type;
int place_points_collected = 0;
int place_px[3];
int place_py[3];

bool blink_state = true;

void clear_canvas() {
  for (int i = 0; i < HEIGHT; i++) {
    for (int j = 0; j < WIDTH; j++) {
      canvas[i][j] = BG_CHAR | COLOR_PAIR(1);
    }
  }
}

void print_canvas() {
  for (int i = 0; i < HEIGHT; i++) {
    for (int j = 0; j < WIDTH; j++) {
      mvaddch(i, j, canvas[i][j]);
    }
  }
  refresh();
}

bool render_direct_mode = false;
int render_direct_attr = 0;

void draw_pixel(int x, int y) {
  if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
    if (render_direct_mode) {
      attron(render_direct_attr | current_render_chtype);
      mvaddch(y, x, FG_CHAR);
      attroff(render_direct_attr | current_render_chtype);
    } else {
      canvas[y][x] = FG_CHAR | current_render_chtype;
    }
  }
}

void draw_line(int x1, int y1, int x2, int y2) {
  int dx = abs(x2 - x1);
  int dy = abs(y2 - y1);
  int sx = x1 < x2 ? 1 : -1;
  int sy = y1 < y2 ? 1 : -1;
  int err = (dx > dy ? dx : -dy) / 2;
  int e2;

  while (1) {
    draw_pixel(x1, y1);
    if (x1 == x2 && y1 == y2)
      break;
    e2 = err;
    if (e2 > -dx) {
      err -= dy;
      x1 += sx;
    }
    if (e2 < dy) {
      err += dx;
      y1 += sy;
    }
  }
}

void draw_rect(int x, int y, int w, int h) {
  draw_line(x, y, x + w, y);
  draw_line(x, y, x, y + h);
  draw_line(x + w, y, x + w, y + h);
  draw_line(x, y + h, x + w, y + h);
}

void draw_circle(int cx, int cy, int radius) {
  int rx = radius * 2;
  int ry = radius;
  if (rx == 0 || ry == 0) {
    draw_pixel(cx, cy);
    return;
  }
  long x = 0, y = ry;
  long rx2 = rx * rx, ry2 = ry * ry;
  long dx = 2 * ry2 * x, dy = 2 * rx2 * y;
  long err = ry2 - (rx2 * ry) + (rx2 / 4);

  while (dx < dy) {
    draw_pixel(cx + x, cy + y);
    draw_pixel(cx - x, cy + y);
    draw_pixel(cx + x, cy - y);
    draw_pixel(cx - x, cy - y);
    if (err < 0) {
      x++;
      dx += 2 * ry2;
      err += dx + ry2;
    } else {
      x++;
      y--;
      dx += 2 * ry2;
      dy -= 2 * rx2;
      err += dx - dy + ry2;
    }
  }

  err = ry2 * (x * x + x) + rx2 * (y * y - 2 * y + 1) - rx2 * ry2;
  while (y >= 0) {
    draw_pixel(cx + x, cy + y);
    draw_pixel(cx - x, cy + y);
    draw_pixel(cx + x, cy - y);
    draw_pixel(cx - x, cy - y);
    if (err > 0) {
      y--;
      dy -= 2 * rx2;
      err += rx2 - dy;
    } else {
      y--;
      x++;
      dx += 2 * ry2;
      dy -= 2 * rx2;
      err += dx - dy + rx2;
    }
  }
}

void draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3) {
  draw_line(x1, y1, x2, y2);
  draw_line(x2, y2, x3, y3);
  draw_line(x3, y3, x1, y1);
}

void draw_filled_rect(int x, int y, int w, int h) {
  for (int i = 0; i <= h; i++) {
    draw_line(x, y + i, x + w, y + i);
  }
}

void draw_filled_circle(int cx, int cy, int radius) {
  for (int y = -radius; y <= radius; y++) {
    int dx = 0;
    while ((dx + 1) * (dx + 1) + y * y <= radius * radius) {
      dx++;
    }
    draw_line(cx - dx, cy + y, cx + dx, cy + y);
  }
}

int point_in_triangle(int px, int py, int x1, int y1, int x2, int y2, int x3,
                      int y3) {
  int denominator = ((y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3));
  if (denominator == 0)
    return 0;
  float a =
      ((y2 - y3) * (px - x3) + (x3 - x2) * (py - y3)) / (float)denominator;
  float b =
      ((y3 - y1) * (px - x3) + (x1 - x3) * (py - y3)) / (float)denominator;
  float c = 1.0f - a - b;
  return a >= 0 && a <= 1 && b >= 0 && b <= 1 && c >= 0 && c <= 1;
}

void draw_filled_triangle(int x1, int y1, int x2, int y2, int x3, int y3) {
  int min_x = x1 < x2 ? (x1 < x3 ? x1 : x3) : (x2 < x3 ? x2 : x3);
  int max_x = x1 > x2 ? (x1 > x3 ? x1 : x3) : (x2 > x3 ? x2 : x3);
  int min_y = y1 < y2 ? (y1 < y3 ? y1 : y3) : (y2 < y3 ? y2 : y3);
  int max_y = y1 > y2 ? (y1 > y3 ? y1 : y3) : (y2 > y3 ? y2 : y3);

  for (int y = min_y; y <= max_y; y++) {
    for (int x = min_x; x <= max_x; x++) {
      if (point_in_triangle(x, y, x1, y1, x2, y2, x3, y3)) {
        draw_pixel(x, y);
      }
    }
  }
}

void render_single_shape(Shape *s) {
  Shape t = *s;

  if (t.type == SHAPE_LINE) {
    t.data.line.x1 = (int)((t.data.line.x1 - pan_x) * zoom);
    t.data.line.y1 = (int)((t.data.line.y1 - pan_y) * zoom);
    t.data.line.x2 = (int)((t.data.line.x2 - pan_x) * zoom);
    t.data.line.y2 = (int)((t.data.line.y2 - pan_y) * zoom);
  } else if (t.type == SHAPE_RECTANGLE) {
    t.data.rect.x = (int)((t.data.rect.x - pan_x) * zoom);
    t.data.rect.y = (int)((t.data.rect.y - pan_y) * zoom);
    t.data.rect.width = (int)(t.data.rect.width * zoom);
    t.data.rect.height = (int)(t.data.rect.height * zoom);
  } else if (t.type == SHAPE_CIRCLE) {
    t.data.circle.cx = (int)((t.data.circle.cx - pan_x) * zoom);
    t.data.circle.cy = (int)((t.data.circle.cy - pan_y) * zoom);
    t.data.circle.r = (int)(t.data.circle.r * zoom);
  } else if (t.type == SHAPE_TRIANGLE) {
    t.data.triangle.x1 = (int)((t.data.triangle.x1 - pan_x) * zoom);
    t.data.triangle.y1 = (int)((t.data.triangle.y1 - pan_y) * zoom);
    t.data.triangle.x2 = (int)((t.data.triangle.x2 - pan_x) * zoom);
    t.data.triangle.y2 = (int)((t.data.triangle.y2 - pan_y) * zoom);
    t.data.triangle.x3 = (int)((t.data.triangle.x3 - pan_x) * zoom);
    t.data.triangle.y3 = (int)((t.data.triangle.y3 - pan_y) * zoom);
  }

  current_render_chtype = COLOR_PAIR(t.color_pair);

  switch (t.type) {
  case SHAPE_LINE:
    draw_line(t.data.line.x1, t.data.line.y1, t.data.line.x2, t.data.line.y2);
    break;
  case SHAPE_RECTANGLE:
    if (t.is_filled) {
      draw_filled_rect(t.data.rect.x, t.data.rect.y, t.data.rect.width,
                       t.data.rect.height);
    } else {
      draw_rect(t.data.rect.x, t.data.rect.y, t.data.rect.width,
                t.data.rect.height);
    }
    break;
  case SHAPE_CIRCLE:
    if (t.is_filled) {
      draw_filled_circle(t.data.circle.cx, t.data.circle.cy, t.data.circle.r);
    } else {
      draw_circle(t.data.circle.cx, t.data.circle.cy, t.data.circle.r);
    }
    break;
  case SHAPE_TRIANGLE:
    if (t.is_filled) {
      draw_filled_triangle(t.data.triangle.x1, t.data.triangle.y1,
                           t.data.triangle.x2, t.data.triangle.y2,
                           t.data.triangle.x3, t.data.triangle.y3);
    } else {
      draw_triangle(t.data.triangle.x1, t.data.triangle.y1, t.data.triangle.x2,
                    t.data.triangle.y2, t.data.triangle.x3, t.data.triangle.y3);
    }
    break;
  }
}

void render_shapes() {
  clear_canvas();
  for (int i = 0; i < shape_count; i++) {
    if ((current_mode == MODE_MENU_DELETE ||
         current_mode == MODE_MENU_MODIFY) &&
        i == menu_selection && !blink_state) {
      continue;
    }
    render_single_shape(&shapes[i]);
  }

  // Live Preview
  if (current_mode == MODE_INTERACTIVE_PLACE && place_points_collected > 0) {
    Shape preview;
    preview.type = place_shape_type;
    preview.color_pair = active_color;
    preview.is_filled = active_fill;
    bool valid = false;

    if (place_shape_type == SHAPE_LINE && place_points_collected == 1) {
      preview.data.line.x1 = place_px[0];
      preview.data.line.y1 = place_py[0];
      preview.data.line.x2 = screen_to_world_x(cursor_x);
      preview.data.line.y2 = screen_to_world_y(cursor_y);
      valid = true;
    } else if (place_shape_type == SHAPE_RECTANGLE &&
               place_points_collected == 1) {
      int wx = screen_to_world_x(cursor_x);
      int wy = screen_to_world_y(cursor_y);
      preview.data.rect.x = place_px[0] < wx ? place_px[0] : wx;
      preview.data.rect.y = place_py[0] < wy ? place_py[0] : wy;
      preview.data.rect.width = abs(wx - place_px[0]);
      preview.data.rect.height = abs(wy - place_py[0]);
      valid = true;
    } else if (place_shape_type == SHAPE_CIRCLE &&
               place_points_collected == 1) {
      int wx = screen_to_world_x(cursor_x);
      int wy = screen_to_world_y(cursor_y);
      preview.data.circle.cx = place_px[0];
      preview.data.circle.cy = place_py[0];
      int dx = wx - place_px[0];
      int dy = wy - place_py[0];
      dx = dx / 2;
      int r2 = dx * dx + dy * dy;
      int r = 0;
      while ((r + 1) * (r + 1) <= r2)
        r++;
      preview.data.circle.r = r;
      valid = true;
    } else if (place_shape_type == SHAPE_TRIANGLE) {
      if (place_points_collected == 1) {
        Shape tmp_line;
        tmp_line.type = SHAPE_LINE;
        tmp_line.data.line.x1 = place_px[0];
        tmp_line.data.line.y1 = place_py[0];
        tmp_line.data.line.x2 = screen_to_world_x(cursor_x);
        tmp_line.data.line.y2 = screen_to_world_y(cursor_y);
        render_single_shape(&tmp_line);
      } else if (place_points_collected == 2) {
        preview.data.triangle.x1 = place_px[0];
        preview.data.triangle.y1 = place_py[0];
        preview.data.triangle.x2 = place_px[1];
        preview.data.triangle.y2 = place_py[1];
        preview.data.triangle.x3 = screen_to_world_x(cursor_x);
        preview.data.triangle.y3 = screen_to_world_y(cursor_y);
        valid = true;
      }
    }

    if (valid)
      render_single_shape(&preview);
  }
}

void print_menu_item(int row, const char *text, bool is_selected) {
  if (is_selected) {
    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(row, 2, " %-74s", text);
    attroff(COLOR_PAIR(2) | A_BOLD);
  } else {
    mvprintw(row, 2, " %-74s", text);
  }
}

void draw_ui() {
  attron(COLOR_PAIR(1));
  for (int i = 0; i < WIDTH; i++) {
    mvaddch(HEIGHT, i, ACS_HLINE);
  }
  for (int i = HEIGHT + 1; i < HEIGHT + 9; i++) {
    mvaddch(i, 0, ACS_VLINE);
    mvaddch(i, WIDTH - 1, ACS_VLINE);
  }
  mvaddch(HEIGHT, 0, ACS_LTEE);
  mvaddch(HEIGHT, WIDTH - 1, ACS_RTEE);
  for (int i = 0; i < WIDTH; i++) {
    mvaddch(HEIGHT + 9, i, ACS_HLINE);
  }
  mvaddch(HEIGHT + 9, 0, ACS_LLCORNER);
  mvaddch(HEIGHT + 9, WIDTH - 1, ACS_LRCORNER);
  attroff(COLOR_PAIR(1));

  attron(COLOR_PAIR(3) | A_BOLD);
  mvprintw(HEIGHT, 2, "[ Zoom: %.1fx | Pan: %d,%d ] [ Color: %d | Fill: %s ]",
           zoom, pan_x, pan_y, active_color, active_fill ? "ON" : "OFF");
  attroff(COLOR_PAIR(3) | A_BOLD);

  if (current_mode == MODE_MENU_MAIN) {
    mvprintw(HEIGHT + 1, 2, "=== Main Menu ===");
    attroff(COLOR_PAIR(3) | A_BOLD);
    print_menu_item(HEIGHT + 2, " 1. Add Shape", menu_selection == 0);
    print_menu_item(HEIGHT + 3, " 2. Modify Shape", menu_selection == 1);
    print_menu_item(HEIGHT + 4, " 3. Delete Shape", menu_selection == 2);
    print_menu_item(HEIGHT + 5, " 4. List Shapes", menu_selection == 3);
    print_menu_item(HEIGHT + 6, " 5. Exit", menu_selection == 4);
  } else if (current_mode == MODE_MENU_ADD) {
    mvprintw(HEIGHT + 1, 2, "=== Select Shape ===");
    attroff(COLOR_PAIR(3) | A_BOLD);
    print_menu_item(HEIGHT + 2, " Line", menu_selection == 0);
    print_menu_item(HEIGHT + 3, " Rectangle", menu_selection == 1);
    print_menu_item(HEIGHT + 4, " Circle", menu_selection == 2);
    print_menu_item(HEIGHT + 5, " Triangle", menu_selection == 3);
    print_menu_item(HEIGHT + 6, " Back", menu_selection == 4);
  } else if (current_mode == MODE_INTERACTIVE_PLACE) {
    mvprintw(HEIGHT + 1, 2, "=== Placing Shape ===");
    attroff(COLOR_PAIR(3) | A_BOLD);
    mvprintw(HEIGHT + 2, 2,
             "Use ARROW KEYS to move cursor. Press ENTER to set point.");
    mvprintw(HEIGHT + 3, 2, "Press ESC to cancel.");
    mvprintw(HEIGHT + 5, 2, "Points placed: %d", place_points_collected);

    // Render Cursor
    int ch = mvinch(cursor_y, cursor_x) & A_CHARTEXT;
    attron(A_REVERSE);
    mvaddch(cursor_y, cursor_x, ch == ' ' ? '+' : ch);
    attroff(A_REVERSE);
  } else if (current_mode == MODE_INTERACTIVE_MODIFY) {
    mvprintw(HEIGHT + 1, 2, "=== Modifying Shape ===");
    attroff(COLOR_PAIR(3) | A_BOLD);
    mvprintw(HEIGHT + 2, 2, "Arrows: Move | +/-: Resize | C: Color | F: Fill");
    mvprintw(HEIGHT + 3, 2, "Press ENTER to confirm. ESC to cancel.");
  } else if (current_mode == MODE_MENU_LIST ||
             current_mode == MODE_MENU_DELETE ||
             current_mode == MODE_MENU_MODIFY) {
    if (current_mode == MODE_MENU_LIST)
      mvprintw(HEIGHT + 1, 2, "=== Shape List ===");
    if (current_mode == MODE_MENU_DELETE)
      mvprintw(HEIGHT + 1, 2, "=== Delete Shape ===");
    if (current_mode == MODE_MENU_MODIFY)
      mvprintw(HEIGHT + 1, 2, "=== Modify Shape ===");
    attroff(COLOR_PAIR(3) | A_BOLD);

    if (shape_count == 0) {
      mvprintw(HEIGHT + 2, 0, "No shapes. Press any key to return.");
      return;
    }

    // Show max 5 items
    for (int i = 0; i < 5; i++) {
      int idx = menu_scroll + i;
      if (idx < shape_count) {
        Shape s = shapes[idx];
        char buf[128];
        if (s.type == SHAPE_LINE)
          sprintf(buf, "ID: %d - Line", s.id);
        else if (s.type == SHAPE_RECTANGLE)
          sprintf(buf, "ID: %d - Rect", s.id);
        else if (s.type == SHAPE_CIRCLE)
          sprintf(buf, "ID: %d - Circle", s.id);
        else if (s.type == SHAPE_TRIANGLE)
          sprintf(buf, "ID: %d - Triangle", s.id);

        print_menu_item(HEIGHT + 2 + i, buf,
                        (current_mode != MODE_MENU_LIST) &&
                            (menu_selection == idx));
      } else {
        move(HEIGHT + 2 + i, 0);
        clrtoeol();
      }
    }
  }
}

void finalize_placement() {
  Shape new_shape;
  new_shape.id = next_id++;
  new_shape.type = place_shape_type;

  if (place_shape_type == SHAPE_LINE) {
    new_shape.data.line.x1 = place_px[0];
    new_shape.data.line.y1 = place_py[0];
    new_shape.data.line.x2 = place_px[1];
    new_shape.data.line.y2 = place_py[1];
  } else if (place_shape_type == SHAPE_RECTANGLE) {
    new_shape.data.rect.x =
        place_px[0] < place_px[1] ? place_px[0] : place_px[1];
    new_shape.data.rect.y =
        place_py[0] < place_py[1] ? place_py[0] : place_py[1];
    new_shape.data.rect.width = abs(place_px[1] - place_px[0]);
    new_shape.data.rect.height = abs(place_py[1] - place_py[0]);
  } else if (place_shape_type == SHAPE_CIRCLE) {
    new_shape.data.circle.cx = place_px[0];
    new_shape.data.circle.cy = place_py[0];
    int dx = place_px[1] - place_px[0];
    int dy = place_py[1] - place_py[0];
    dx = dx / 2;
    int r2 = dx * dx + dy * dy;
    int r = 0;
    while ((r + 1) * (r + 1) <= r2)
      r++;
    new_shape.data.circle.r = r;
  } else if (place_shape_type == SHAPE_TRIANGLE) {
    new_shape.data.triangle.x1 = place_px[0];
    new_shape.data.triangle.y1 = place_py[0];
    new_shape.data.triangle.x2 = place_px[1];
    new_shape.data.triangle.y2 = place_py[1];
    new_shape.data.triangle.x3 = place_px[2];
    new_shape.data.triangle.y3 = place_py[2];
  }

  new_shape.color_pair = active_color;
  new_shape.is_filled = active_fill;

  if (shape_count < MAX_SHAPES) {
    save_state();
    shapes[shape_count++] = new_shape;
  }

  current_mode = MODE_MENU_MAIN;
  menu_selection = 0;
}

void translate_shape(Shape *s, int dx, int dy) {
  if (s->type == SHAPE_LINE) {
    s->data.line.x1 += dx;
    s->data.line.x2 += dx;
    s->data.line.y1 += dy;
    s->data.line.y2 += dy;
  } else if (s->type == SHAPE_RECTANGLE) {
    s->data.rect.x += dx;
    s->data.rect.y += dy;
  } else if (s->type == SHAPE_CIRCLE) {
    s->data.circle.cx += dx;
    s->data.circle.cy += dy;
  } else if (s->type == SHAPE_TRIANGLE) {
    s->data.triangle.x1 += dx;
    s->data.triangle.x2 += dx;
    s->data.triangle.x3 += dx;
    s->data.triangle.y1 += dy;
    s->data.triangle.y2 += dy;
    s->data.triangle.y3 += dy;
  }
}

void scale_shape(Shape *s, int delta) {
  if (s->type == SHAPE_LINE) {
    int dx = s->data.line.x2 - s->data.line.x1;
    int dy = s->data.line.y2 - s->data.line.y1;
    if (dx == 0 && dy == 0) return;
    if (dx > 0) s->data.line.x2 += delta; else if (dx < 0) s->data.line.x2 -= delta;
    if (dy > 0) s->data.line.y2 += delta; else if (dy < 0) s->data.line.y2 -= delta;
  } else if (s->type == SHAPE_RECTANGLE) {
    s->data.rect.width += delta;
    s->data.rect.height += delta;
    if (s->data.rect.width < 1) s->data.rect.width = 1;
    if (s->data.rect.height < 1) s->data.rect.height = 1;
  } else if (s->type == SHAPE_CIRCLE) {
    s->data.circle.r += delta;
    if (s->data.circle.r < 1) s->data.circle.r = 1;
  } else if (s->type == SHAPE_TRIANGLE) {
    int cx = (s->data.triangle.x1 + s->data.triangle.x2 + s->data.triangle.x3) / 3;
    int cy = (s->data.triangle.y1 + s->data.triangle.y2 + s->data.triangle.y3) / 3;
    if (s->data.triangle.x1 > cx) s->data.triangle.x1 += delta; else if (s->data.triangle.x1 < cx) s->data.triangle.x1 -= delta;
    if (s->data.triangle.y1 > cy) s->data.triangle.y1 += delta; else if (s->data.triangle.y1 < cy) s->data.triangle.y1 -= delta;
    if (s->data.triangle.x2 > cx) s->data.triangle.x2 += delta; else if (s->data.triangle.x2 < cx) s->data.triangle.x2 -= delta;
    if (s->data.triangle.y2 > cy) s->data.triangle.y2 += delta; else if (s->data.triangle.y2 < cy) s->data.triangle.y2 -= delta;
    if (s->data.triangle.x3 > cx) s->data.triangle.x3 += delta; else if (s->data.triangle.x3 < cx) s->data.triangle.x3 -= delta;
    if (s->data.triangle.y3 > cy) s->data.triangle.y3 += delta; else if (s->data.triangle.y3 < cy) s->data.triangle.y3 -= delta;
  }
}

int main() {
#ifdef _WIN32
  HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode;
  if (GetConsoleMode(hStdin, &mode)) {
    mode &= ~ENABLE_QUICK_EDIT_MODE;
    mode |= ENABLE_MOUSE_INPUT;
    SetConsoleMode(hStdin, mode | ENABLE_EXTENDED_FLAGS);
  }
#endif

  initscr();
  printf("\033[?1003h\033[?1015h\033[?1006h");
  fflush(stdout);
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0); // Hide actual terminal cursor
  timeout(250);

  if (has_colors()) {
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);
    init_pair(5, COLOR_BLUE, COLOR_BLACK);
    init_pair(6, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(7, COLOR_CYAN, COLOR_BLACK);
  }

  mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);

  save_state();

  int blink_counter = 0;

  while (1) {
    blink_state = (blink_counter % 2) == 0;
    render_shapes();
    clear();
    print_canvas();

    // Highlight selected shape if modifying or deleting
    if ((current_mode == MODE_MENU_DELETE ||
         current_mode == MODE_MENU_MODIFY) &&
        shape_count > 0 && blink_state) {
      render_direct_mode = true;
      render_direct_attr = A_BOLD;
      if (menu_selection >= 0 && menu_selection < shape_count) {
        render_single_shape(&shapes[menu_selection]);
      }
      render_direct_mode = false;
    }

    draw_ui();

    int ch = getch();
    if (ch == ERR) {
      blink_counter++;
      continue;
    } else {
      blink_counter = 0;
    }

    if (ch == KEY_MOUSE) {
      MEVENT ev;
      if (getmouse(&ev) == OK) {
        bool just_clicked = (ev.bstate & BUTTON1_CLICKED) != 0;

        if (ev.y < HEIGHT && ev.x < WIDTH) {
          cursor_x = ev.x;
          cursor_y = ev.y;
          if (just_clicked) { 
            ch = '\n';     // Simulate enter key for placement
          }
        } else if (ev.y >= HEIGHT + 2 && ev.y < HEIGHT + 2 + 5) {
          int hover_idx = ev.y - (HEIGHT + 2);
          if (current_mode == MODE_MENU_MAIN || current_mode == MODE_MENU_ADD) {
            menu_selection = hover_idx;
            if (just_clicked) ch = '\n';
          } else if (current_mode == MODE_MENU_LIST || current_mode == MODE_MENU_DELETE || current_mode == MODE_MENU_MODIFY) {
            int actual_idx = menu_scroll + hover_idx;
            if (actual_idx < shape_count) {
              menu_selection = actual_idx;
              if (just_clicked) ch = '\n';
            }
          }
        }
      }
    }
    
    if (ch == 'u' || ch == 'U') {
      do_undo();
      continue;
    } else if (ch == 'r' || ch == 'R') {
      do_redo();
      continue;
    } else if ((ch == 'c' || ch == 'C') && current_mode != MODE_INTERACTIVE_MODIFY) {
      active_color = (active_color % 7) + 1;
      continue;
    } else if ((ch == 'f' || ch == 'F') && current_mode != MODE_INTERACTIVE_MODIFY) {
      active_fill = !active_fill;
      continue;
    } else if ((ch == '=' || ch == '+') && current_mode != MODE_INTERACTIVE_MODIFY) {
      zoom += 0.2f;
      continue;
    } else if (ch == '-' && current_mode != MODE_INTERACTIVE_MODIFY) {
      if (zoom > 0.3f)
        zoom -= 0.2f;
      continue;
    } else if (ch == 'w' || ch == 'W') {
      pan_y--;
      continue;
    } else if (ch == 's' || ch == 'S') {
      pan_y++;
      continue;
    } else if (ch == 'a' || ch == 'A') {
      pan_x--;
      continue;
    } else if (ch == 'd' || ch == 'D') {
      pan_x++;
      continue;
    }

    if (current_mode == MODE_MENU_MAIN) {
      if (ch == KEY_UP)
        menu_selection = (menu_selection - 1 + 5) % 5;
      else if (ch == KEY_DOWN)
        menu_selection = (menu_selection + 1) % 5;
      else if (ch == '\n' || ch == KEY_ENTER) {
        if (menu_selection == 0) {
          current_mode = MODE_MENU_ADD;
          menu_selection = 0;
        } else if (menu_selection == 1) {
          current_mode = MODE_MENU_MODIFY;
          menu_selection = 0;
          menu_scroll = 0;
        } else if (menu_selection == 2) {
          current_mode = MODE_MENU_DELETE;
          menu_selection = 0;
          menu_scroll = 0;
        } else if (menu_selection == 3) {
          current_mode = MODE_MENU_LIST;
        } else if (menu_selection == 4) {
          endwin();
          return 0;
        }
      }
    } else if (current_mode == MODE_MENU_ADD) {
      if (ch == KEY_UP)
        menu_selection = (menu_selection - 1 + 5) % 5;
      else if (ch == KEY_DOWN)
        menu_selection = (menu_selection + 1) % 5;
      else if (ch == '\n' || ch == KEY_ENTER) {
        if (menu_selection == 4) {
          current_mode = MODE_MENU_MAIN;
          menu_selection = 0;
        } else {
          place_shape_type = (ShapeType)menu_selection;
          place_points_collected = 0;
          current_mode = MODE_INTERACTIVE_PLACE;
          cursor_x = WIDTH / 2;
          cursor_y = HEIGHT / 2;
        }
      }
    } else if (current_mode == MODE_INTERACTIVE_PLACE) {
      if (ch == KEY_UP && cursor_y > 0)
        cursor_y--;
      else if (ch == KEY_DOWN && cursor_y < HEIGHT - 1)
        cursor_y++;
      else if (ch == KEY_LEFT && cursor_x > 0)
        cursor_x--;
      else if (ch == KEY_RIGHT && cursor_x < WIDTH - 1)
        cursor_x++;
      else if (ch == 27) { // ESC
        current_mode = MODE_MENU_MAIN;
        menu_selection = 0;
      } else if (ch == '\n' || ch == KEY_ENTER) {
        place_px[place_points_collected] = cursor_x;
        place_py[place_points_collected] = cursor_y;
        place_points_collected++;

        int required_points = (place_shape_type == SHAPE_TRIANGLE) ? 3 : 2;
        if (place_points_collected >= required_points) {
          finalize_placement();
        }
      }
    } else if (current_mode == MODE_INTERACTIVE_MODIFY) {
      int dx = 0, dy = 0;
      int scale_delta = 0;
      if (ch == KEY_UP)
        dy = -1;
      else if (ch == KEY_DOWN)
        dy = 1;
      else if (ch == KEY_LEFT)
        dx = -1;
      else if (ch == KEY_RIGHT)
        dx = 1;
      else if (ch == '+' || ch == '=')
        scale_delta = 1;
      else if (ch == '-' || ch == '_')
        scale_delta = -1;
      else if (ch == 'c' || ch == 'C') {
        shapes[menu_selection].color_pair++;
        if (shapes[menu_selection].color_pair > 7) shapes[menu_selection].color_pair = 1;
      } else if (ch == 'f' || ch == 'F') {
        shapes[menu_selection].is_filled = !shapes[menu_selection].is_filled;
      }
      else if (ch == 27) { // ESC
        do_undo();
        current_mode = MODE_MENU_MAIN;
        menu_selection = 0;
      } else if (ch == '\n' || ch == KEY_ENTER) {
        current_mode = MODE_MENU_MAIN;
        menu_selection = 0;
      }

      if (dx != 0 || dy != 0) {
        translate_shape(&shapes[menu_selection], dx, dy);
      }
      if (scale_delta != 0) {
        scale_shape(&shapes[menu_selection], scale_delta);
      }
    } else if (current_mode == MODE_MENU_LIST ||
               current_mode == MODE_MENU_DELETE ||
               current_mode == MODE_MENU_MODIFY) {
      if (shape_count == 0 || current_mode == MODE_MENU_LIST) {
        if (ch != ERR) {
          current_mode = MODE_MENU_MAIN;
          menu_selection = 0;
        }
        continue;
      }

      if (ch == KEY_UP) {
        if (menu_selection > 0)
          menu_selection--;
        if (menu_selection < menu_scroll)
          menu_scroll = menu_selection;
      } else if (ch == KEY_DOWN) {
        if (menu_selection < shape_count - 1)
          menu_selection++;
        if (menu_selection >= menu_scroll + 5)
          menu_scroll = menu_selection - 4;
      } else if (ch == 27) { // ESC
        current_mode = MODE_MENU_MAIN;
        menu_selection = 0;
      } else if (ch == '\n' || ch == KEY_ENTER) {
        if (current_mode == MODE_MENU_DELETE) {
          save_state();
          for (int i = menu_selection; i < shape_count - 1; i++) {
            shapes[i] = shapes[i + 1];
          }
          shape_count--;
          current_mode = MODE_MENU_MAIN;
          menu_selection = 0;
        } else if (current_mode == MODE_MENU_MODIFY) {
          save_state();
          current_mode = MODE_INTERACTIVE_MODIFY;
        }
      }
    }
  }

  endwin();
  return 0;
}
