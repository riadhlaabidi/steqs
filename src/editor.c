#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _POSIX_C_SOURCE >= 200809L
#define _GNU_SOURCE

#include "editor.h"
#include "append_buffer.h"
#include "kbd.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

editor_config ec;

void init_editor()
{

    enable_raw_mode();

    ec.cx = 0;
    ec.cy = 0;
    ec.rx = 0;
    ec.num_trows = 0;
    ec.t_rows = NULL;
    ec.row_offset = 0;
    ec.col_offset = 0;

    if (get_window_size(&ec.rows, &ec.cols) == -1) {
        DIE("Unable to get window size");
    }
}

int get_cursor_pos(int *rows, int *cols)
{
    char buf[32];
    size_t i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
        return -1;
    }

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) == -1) {
            return -1;
        }

        if (buf[i] == 'R') {
            break;
        }
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') {
        return -1;
    }

    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) {
        return -1;
    }

    return 0;
}

void move_cursor(int key)
{
    text_row *row = NULL;

    if (ec.cy < ec.num_trows) {
        row = &ec.t_rows[ec.cy];
    }

    switch (key) {
        case ARROW_UP:
            if (ec.cy > 0) {
                ec.cy--;
            }
            break;
        case ARROW_DOWN:
            if (ec.cy < ec.num_trows - 1) {
                ec.cy++;
            }
            break;
        case ARROW_LEFT:
            if (ec.cx > 0) {
                ec.cx--;
            } else if (ec.cy > 0) {
                ec.cy--;
                int rs = ec.t_rows[ec.cy].size;
                if (rs > 0) {
                    ec.cx = rs - 1;
                }
            }
            break;
        case ARROW_RIGHT:
            if (row && ec.cx < row->size - 1) {
                ec.cx++;
            } else {
                if (row) {
                    if (row->size == 0) {
                        if (ec.cy < ec.num_trows - 1) {
                            ec.cy++;
                            ec.cx = 0;
                        }
                    } else {
                        if (ec.cy < ec.num_trows - 1) {
                            ec.cy++;
                            ec.cx = 0;
                        }
                    }
                }
            }
            break;
    }

    // change cursor pos to the end of the current row
    // if the actual cursor pos is bigger than row's length.
    if (ec.cy < ec.num_trows) {
        row = &ec.t_rows[ec.cy];
    }

    int row_len = row ? row->size : 0;

    if (ec.cx > row_len) {
        ec.cx = row_len;
    }
}

int get_window_size(int *rows, int *cols)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
            return -1;
        }
        return get_cursor_pos(rows, cols);
    }

    *rows = ws.ws_row;
    *cols = ws.ws_col;
    return 0;
}

void open_file(char *filename)
{
    FILE *fp = fopen(filename, "r");

    if (!fp) {
        DIE("Failed to open file");
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t line_len;

    while ((line_len = getline(&line, &linecap, fp)) != -1) {
        while (line_len > 0 &&
               (line[line_len - 1] == '\n' || line[line_len - 1] == '\r')) {
            line_len--;
        }
        append_text_row(line, line_len);
    }
    FREE(line);
    fclose(fp);
}

void append_text_row(char *content, size_t len)
{
    ec.t_rows = realloc(ec.t_rows, sizeof(text_row) * (ec.num_trows + 1));

    // Last index
    int idx = ec.num_trows;

    ec.t_rows[idx].size = len;
    ec.t_rows[idx].content = malloc(len + 1);

    memcpy(ec.t_rows[idx].content, content, len);

    ec.t_rows[idx].content[len] = '\0';
    ec.t_rows[idx].render_size = 0;
    ec.t_rows[idx].to_render = NULL;
    update_text_row(&ec.t_rows[idx]);

    ec.num_trows++;
}

void update_text_row(text_row *row)
{
    int tabs = 0;
    int i;

    for (i = 0; i < tabs; i++) {
        if (row->content[i] == '\t') {
            tabs++;
        }
    }

    free(row->to_render);
    row->to_render = malloc(row->size + tabs * (TAB_STOP - 1) + 1);

    int idx = 0;
    for (i = 0; i < row->size; i++) {
        if (row->content[i] == '\t') {
            while (idx % TAB_STOP != 0) {
                row->to_render[idx++] = ' ';
            }
        } else {
            row->to_render[idx++] = row->content[i];
        }
    }

    row->to_render[idx] = '\0';
    row->render_size = idx;
}

void draw_row_tildes(abuf *buf)
{
    int i;

    for (i = 0; i < ec.rows; i++) {
        int file_row = i + ec.row_offset;
        if (ec.num_trows > file_row) {
            int len = ec.t_rows[file_row].render_size - ec.col_offset;
            if (len < 0) {
                len = 0;
            }
            if (len > ec.cols) {
                len = ec.cols;
            }
            buf_append(buf, &ec.t_rows[file_row].to_render[ec.col_offset], len);
        } else {
            buf_append(buf, "~", 1);
            if (ec.num_trows == 0 && i == ec.rows / 3) {
                char welcome[30];
                int welcome_len =
                    snprintf(welcome, sizeof(welcome), " %s - Version %s",
                             EDITOR_NAME, EDITOR_VERSION);
                int padding = (ec.cols - welcome_len - 1) / 2;

                while (padding > 0) {
                    buf_append(buf, " ", 1);
                    padding--;
                }

                buf_append(buf, welcome, welcome_len);
            }
        }

        // erase from the active position to the end of line.
        // default param 0
        buf_append(buf, "\x1b[K", 3);

        if (i < ec.rows - 1) {
            buf_append(buf, "\r\n", 2);
        }
    }
}

void scroll()
{
    ec.rx = ec.cx;
    if (ec.cy < ec.row_offset) {
        ec.row_offset = ec.cy;
    }
    if (ec.cy >= ec.row_offset + ec.rows) {
        ec.row_offset = ec.cy - ec.rows + 1;
    }
    if (ec.rx < ec.col_offset) {
        ec.col_offset = ec.rx;
    }
    if (ec.rx >= ec.col_offset + ec.cols) {
        ec.col_offset = ec.rx - ec.cols + 1;
    }
}

void refresh_screen()
{
    scroll();
    abuf buf = ABUF_INIT;

    // Hide the cursor to get rid of flickering effect
    buf_append(&buf, "\x1b[?25l", 6);

    // Move cursor to the home position
    buf_append(&buf, "\x1b[H", 3);

    draw_row_tildes(&buf);

    // Move cursor to the home position
    char move_cmd[32];
    snprintf(move_cmd, sizeof(move_cmd), "\x1b[%d;%dH",
             (ec.cy - ec.row_offset) + 1, (ec.rx - ec.col_offset) + 1);
    buf_append(&buf, move_cmd, strlen(move_cmd));

    // Show the cursor
    buf_append(&buf, "\x1b[?25h", 6);

    write(STDOUT_FILENO, buf.buf, buf.len);
    buf_free(&buf);
}

void disable_raw_mode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &ec.default_settings) == -1) {
        DIE("tcsetattr: Unable to set changed terminal settings");
    }
}

void enable_raw_mode()
{
    if (tcgetattr(STDIN_FILENO, &ec.default_settings) == -1) {
        DIE("tcgetattr: Unable to retrieve terminal settings");
    }
    atexit(disable_raw_mode);

    struct termios raw = ec.default_settings;
    cfmakeraw(&raw);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        DIE("tcsetattr: Unable to set changed terminal settings");
    }
}

editor_config *get_editor_config() { return &ec; }

void process_key()
{
    int c = read_key();

    switch (c) {
        case CTRL_KEY('q'):
            // Erase from the active position to the end of line.
            // default param 0
            write(STDOUT_FILENO, "\x1b[2J", 4);
            // Move cursor to the home position
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(EXIT_SUCCESS);
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            move_cursor(c);
            break;
        case PAGE_UP:
        case PAGE_DOWN:
            {
                int i = ec.rows;
                while (i--) {
                    move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
                }
            }
            break;
        case HOME:
            ec.cx = 0;
            break;
        case END:
            ec.cx = ec.cols - 1;
            break;
    }
}
