#include "editor.h"

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
