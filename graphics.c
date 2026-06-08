#include "editor.h"

int screen_to_world_x(int sx) { return (int)(sx / zoom) + pan_x; }
int screen_to_world_y(int sy) { return (int)(sy / zoom) + pan_y; }

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

  if (current_mode == MODE_INTERACTIVE_PLACE && place_points_collected > 0) {
    Shape preview;
    preview.type = place_shape_type;
    preview.color_pair = active_color;
    preview.is_filled = active_fill;
    
    if (place_shape_type == SHAPE_LINE) {
      preview.data.line.x1 = place_px[0];
      preview.data.line.y1 = place_py[0];
      preview.data.line.x2 = cursor_x;
      preview.data.line.y2 = cursor_y;
    } else if (place_shape_type == SHAPE_RECTANGLE) {
      preview.data.rect.x = place_px[0] < cursor_x ? place_px[0] : cursor_x;
      preview.data.rect.y = place_py[0] < cursor_y ? place_py[0] : cursor_y;
      preview.data.rect.width = abs(cursor_x - place_px[0]);
      preview.data.rect.height = abs(cursor_y - place_py[0]);
    } else if (place_shape_type == SHAPE_CIRCLE) {
      preview.data.circle.cx = place_px[0];
      preview.data.circle.cy = place_py[0];
      int dx = cursor_x - place_px[0];
      int dy = cursor_y - place_py[0];
      dx = dx / 2;
      int r2 = dx * dx + dy * dy;
      int r = 0;
      while ((r + 1) * (r + 1) <= r2) r++;
      preview.data.circle.r = r;
    } else if (place_shape_type == SHAPE_TRIANGLE) {
      preview.data.triangle.x1 = place_px[0];
      preview.data.triangle.y1 = place_py[0];
      if (place_points_collected == 1) {
        preview.data.triangle.x2 = cursor_x;
        preview.data.triangle.y2 = cursor_y;
        preview.data.triangle.x3 = cursor_x;
        preview.data.triangle.y3 = cursor_y;
      } else {
        preview.data.triangle.x2 = place_px[1];
        preview.data.triangle.y2 = place_py[1];
        preview.data.triangle.x3 = cursor_x;
        preview.data.triangle.y3 = cursor_y;
      }
    }
    render_single_shape(&preview);
  }
}
