#include "editor.h"

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
