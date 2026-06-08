#include "editor.h"

Shape shapes[MAX_SHAPES];
int shape_count = 0;
int next_id = 1;
chtype canvas[HEIGHT][WIDTH];
int active_color = 1;
bool active_fill = false;
chtype current_render_chtype = 0;

Shape undo_stack[MAX_UNDO][MAX_SHAPES];
int undo_shape_counts[MAX_UNDO];
int undo_current = -1;
int undo_head = -1;

float zoom = 1.0f;
int pan_x = 0;
int pan_y = 0;

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
bool render_direct_mode = false;
int render_direct_attr = 0;

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
