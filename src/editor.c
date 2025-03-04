#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _POSIX_C_SOURCE >= 200809L
#define _GNU_SOURCE

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "append_buffer.h"
#include "cursor.h"
#include "editor.h"
#include "find.h"
#include "highlight.h"
#include "kbd.h"
#include "status_bar.h"
#include "util.h"

editor_config ec;
int quit_times = EDITOR_UNSAVED_QUIT_TIMES;

void init_editor(void)
{

    enable_raw_mode();

    ec.cx = 0;
    ec.cy = 0;
    ec.rx = 0;
    ec.num_trows = 0;
    ec.t_rows = NULL;
    ec.row_offset = 0;
    ec.col_offset = 0;
    ec.filename = NULL;
    ec.status_msg[0] = '\0';
    ec.dirty = 0;
    ec.prompting = 0;
    ec.syntax = NULL;
    ec.line_number_padding = 0;

    if (get_window_size(&ec.rows, &ec.cols) == -1) {
        DIE("Unable to get window size");
    }

    // leave one line for status line and another for status msg
    ec.rows -= 2;

    set_status_msg("Help: ^s Save | ^q Quit | ^f Find");
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
    FREE(ec.filename);
    ec.filename = strdup(filename);

    // select syntax highlighting based on file type
    select_syntax_highlight();

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
        insert_text_row(ec.num_trows, line, line_len);
    }
    FREE(line);
    fclose(fp);
    ec.dirty = 0;
}

void insert_text_row(int pos, char *content, size_t len)
{
    if (pos < 0 || pos > ec.num_trows)
        return;

    ec.t_rows = realloc(ec.t_rows, sizeof(text_row) * (ec.num_trows + 1));

    memmove(&ec.t_rows[pos + 1], &ec.t_rows[pos],
            sizeof(text_row) * (ec.num_trows - pos));

    for (int i = pos + 1; i <= ec.num_trows; ++i) {
        ec.t_rows[i].index++;
    }

    ec.t_rows[pos].index = pos;
    ec.t_rows[pos].size = len;
    ec.t_rows[pos].content = malloc(len + 1);
    memcpy(ec.t_rows[pos].content, content, len);
    ec.t_rows[pos].content[len] = '\0';
    ec.t_rows[pos].render_size = 0;
    ec.t_rows[pos].to_render = NULL;
    ec.t_rows[pos].highlight = NULL;
    ec.t_rows[pos].highlight_open_comment = 0;

    update_text_row(&ec.t_rows[pos]);

    ec.num_trows++;
    ec.dirty++;
}

void delete_text_row(int pos)
{
    if (pos < 0 || pos >= ec.num_trows)
        return;

    free_text_row(&ec.t_rows[pos]);

    memmove(&ec.t_rows[pos], &ec.t_rows[pos + 1],
            sizeof(text_row) * (ec.num_trows - pos - 1));

    for (int i = pos; i < ec.num_trows - 1; ++i) {
        ec.t_rows[i].index--;
    }

    ec.num_trows--;
    ec.dirty++;
}

void free_text_row(text_row *tr)
{
    FREE(tr->content);
    FREE(tr->to_render);
    FREE(tr->highlight);
}

void update_text_row(text_row *row)
{
    int tabs = 0;
    int i;

    for (i = 0; i < row->size; i++) {
        if (row->content[i] == '\t') {
            tabs++;
        }
    }

    int new_size = row->size + tabs * (TAB_STOP - 1) + 1;
    FREE(row->to_render);
    row->to_render = malloc(new_size);

    if (!row->to_render) {
        DIE("Failed to allocate memory");
    }

    int idx = 0;
    for (i = 0; i < row->size; i++) {
        if (row->content[i] == '\t') {
            row->to_render[idx++] = ' ';
            while (idx % TAB_STOP != 0) {
                row->to_render[idx++] = ' ';
            }
        } else {
            row->to_render[idx++] = row->content[i];
        }
    }

    row->to_render[idx] = '\0';
    row->render_size = idx;

    // update syntax for highliting
    update_syntax(row);
}

void draw_line_number(abuf *buf, int line_number)
{
    int left_padding = ec.line_number_padding - count_digits(line_number) - 1;

    while (left_padding > 0) {
        buf_append(buf, " ", 1);
        left_padding--;
    }

    char b[12];
    buf_append(buf, "\x1b[90m", 5); // dark grey foreground
    int len = snprintf(b, sizeof(b), "%d", line_number);
    buf_append(buf, b, len);
    buf_append(buf, "\x1b[39m", 5); // back to default foreground
    buf_append(buf, " ", 1);
}

void draw_row_tildes(abuf *buf)
{
    int i;
    for (i = 0; i < ec.rows; i++) {
        int file_row = i + ec.row_offset;
        if (ec.num_trows > file_row) {
            draw_line_number(buf, file_row + 1);
            int len = ec.t_rows[file_row].render_size - ec.col_offset;
            if (len < 0) {
                len = 0;
            }
            if (len > ec.cols) {
                len = ec.cols;
            }

            char *line = &ec.t_rows[file_row].to_render[ec.col_offset];
            unsigned char *hl = &ec.t_rows[file_row].highlight[ec.col_offset];
            int current_color = -1;
            int j;
            for (j = 0; j < len; j++) {
                if (iscntrl(line[j])) { // non printable characters
                    // non alphabetic control characters are printed as '?'
                    char symbol = '?';
                    if (line[j] <= 26) {
                        // if it is an alphabetic control character we print the
                        // related capital letter (A..Z) which comes after the
                        // '@' character in ASCII
                        symbol = '@' + line[j];
                    }
                    // switch to dark grey foreground
                    buf_append(buf, "\x1b[90m", 5);
                    buf_append(buf, "^", 1);
                    buf_append(buf, &symbol, 1);
                    // switch back to default foreground
                    buf_append(buf, "\x1b[m", 3);
                    if (current_color != -1) {
                        char b[16];
                        int clen =
                            snprintf(b, sizeof(b), "\x1b[%dm", current_color);
                        buf_append(buf, b, clen);
                    }
                } else if (hl[j] == HL_NORMAL) {
                    if (current_color != -1) {
                        buf_append(buf, "\x1b[39m", 5);
                        current_color = -1;
                    }
                    buf_append(buf, &line[j], 1);
                } else {
                    int color = syntax_to_color(hl[j]);
                    if (color != current_color) {
                        current_color = color;
                        char b[16];
                        int clen = snprintf(b, sizeof(b), "\x1b[%dm", color);
                        buf_append(buf, b, clen);
                    }
                    buf_append(buf, &line[j], 1);
                }
            }
            buf_append(buf, "\x1b[39m", 5);
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

        buf_append(buf, "\r\n", 2);
    }
}

/**
 * Converts cursor index from character-based into a rendering-based
 * index after rendering escape character '\t' in a text row
 */
int row_cx_to_rx(text_row *tr, int cx)
{
    int rx = 0;
    int i;

    for (i = 0; i < cx; i++) {
        if (tr->content[i] == '\t') {
            rx += (TAB_STOP - 1) - (rx % TAB_STOP);
        } else if (iscntrl(tr->content[i])) {
            rx++;
        }
        rx++;
    }

    return rx;
}

/**
 * Converts cursor index from rendering-based into a character based index
 * reverting escape character '\t' rendering in a text row.
 */
int row_rx_to_cx(text_row *tr, int rx)
{
    int curr_rx = 0;
    int i;

    for (i = 0; i < tr->size; i++) {
        if (tr->content[i] == '\t') {
            curr_rx += (TAB_STOP - 1) - (curr_rx % TAB_STOP);
        } else if (iscntrl(tr->content[i])) {
            curr_rx++;
        }

        curr_rx++;

        if (curr_rx > rx) {
            // Example: [\t][a][b] would be rendered as [........][a][b]
            // while converting index rx=3 from the rendered content to its
            // character index cx: curr_rx is 8 for i = 0, curr_rx > rx, means
            // that rx points into the 8 space rendered tab character, which is
            // of index i = 0 in the character-based content array, so we stop
            // right there.
            return i;
        }
    }

    return i;
}

void scroll(void)
{
    ec.rx = 0;

    if (ec.cy < ec.num_trows) {
        ec.rx = row_cx_to_rx(&ec.t_rows[ec.cy], ec.cx);
    }

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

void refresh_screen(void)
{
    scroll();
    abuf buf = ABUF_INIT;

    // Hide the cursor to get rid of flickering effect
    buf_append(&buf, "\x1b[?25l", 6);

    if (ec.num_trows) {
        // Two spaces padding before & after the line number
        ec.line_number_padding = count_digits(ec.num_trows) + 2;
        if (ec.line_number_padding < EDITOR_DEFAULT_LINE_NUMBER_PADDING) {
            ec.line_number_padding = EDITOR_DEFAULT_LINE_NUMBER_PADDING;
        }
    }

    // Move cursor to the home position
    buf_append(&buf, "\x1b[H", 3);

    draw_row_tildes(&buf);
    draw_status_bar(&buf);
    draw_message_bar(&buf);

    // cursor position
    int r = (ec.cy - ec.row_offset) + 1;
    int c = (ec.rx - ec.col_offset + ec.line_number_padding) + 1;

    if (ec.prompting) {
        r = ec.rows + 2;
        c = strlen(ec.status_msg) + 1;
    }

    // move cursor to position r, c
    char move_cmd[32];
    snprintf(move_cmd, sizeof(move_cmd), "\x1b[%d;%dH", r, c);
    buf_append(&buf, move_cmd, strlen(move_cmd));

    // Show the cursor
    buf_append(&buf, "\x1b[?25h", 6);

    write(STDOUT_FILENO, buf.buf, buf.len);
    buf_free(&buf);
}

void disable_raw_mode(void)
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &ec.default_settings) == -1) {
        DIE("tcsetattr: Unable to set changed terminal settings");
    }

    // switch back from alternate buffer to main screen
    write(STDOUT_FILENO, "\x1b[?1049l", 8);
}

void enable_raw_mode(void)
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

    // enable alternate buffer
    write(STDOUT_FILENO, "\x1b[?1049h", 8);
}

void process_key(void)
{
    int c = read_key();

    switch (c) {
        case '\r':
            insert_new_line();
            break;
        case CTRL_KEY('q'):
            if (ec.dirty && quit_times > 0) {
                set_status_msg("Warning! File has unsaved changes. Press ^q %d "
                               "more time%s to quit without saving changes.",
                               quit_times, quit_times == 1 ? "" : "s");
                quit_times--;
                return;
            }
            // Erase all of the display – all lines are erased, changed to
            // single-width, and the cursor does not move
            write(STDOUT_FILENO, "\x1b[2J", 4);
            // Move cursor to the home position
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(EXIT_SUCCESS);
            break;
        case CTRL_KEY('s'):
            save();
            break;

        case CTRL_KEY('f'):
            find();
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            move_cursor(c);
            break;
        case PAGE_UP:
            {
                ec.cy = ec.row_offset;

                int i = ec.rows;
                while (i--) {
                    move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
                }
            }
            break;
        case PAGE_DOWN:
            {
                ec.cy = ec.row_offset + ec.rows - 1;

                if (ec.cy > ec.num_trows - 1) {
                    ec.cy = ec.num_trows - 1;
                }

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
            if (ec.cy < ec.num_trows) {
                ec.cx = ec.t_rows[ec.cy].size - 1;
            }
            break;
        case CTRL_KEY('h'):
        case BACKSPACE:
            delete_char();
            break;
        case DEL:
            move_cursor(ARROW_RIGHT);
            delete_char();
            break;
        case CTRL_KEY('l'):
        case '\x1b':
            break;
        default:
            insert_char(c);
            break;
    }

    quit_times = EDITOR_UNSAVED_QUIT_TIMES;
}

void text_row_insert_char(text_row *tr, int pos, int c)
{
    if (pos < 0 || pos > tr->size) {
        pos = tr->size;
    }

    tr->content = realloc(tr->content, tr->size + 2);
    memmove(&tr->content[pos + 1], &tr->content[pos], tr->size - pos + 1);
    tr->content[pos] = c;
    tr->size++;

    update_text_row(tr);
    ec.dirty++;
}

void text_row_delete_char(text_row *tr, int pos)
{
    if (pos < 0 || pos >= tr->size) {
        return;
    }
    memmove(&tr->content[pos], &tr->content[pos + 1], tr->size - pos);
    tr->size--;
    update_text_row(tr);
    ec.dirty++;
}

void text_row_append_string(text_row *tr, char *s, size_t len)
{
    tr->content = realloc(tr->content, tr->size + len + 1);
    memcpy(&tr->content[tr->size], s, len);
    tr->size += len;
    tr->content[tr->size] = '\0';
    update_text_row(tr);
    ec.dirty++;
}

void insert_char(int c)
{
    if (ec.cy == ec.num_trows) {
        insert_text_row(ec.num_trows, "", 0);
    }

    text_row_insert_char(&ec.t_rows[ec.cy], ec.cx, c);
    ec.cx++;
}

void insert_new_line(void)
{
    if (ec.cx == 0) {
        insert_text_row(ec.cy, "", 0);
    } else {
        text_row *curr = &ec.t_rows[ec.cy];
        insert_text_row(ec.cy + 1, &curr->content[ec.cx], curr->size - ec.cx);

        // insert_text_row reallocates the t_rows array, need to reassign curr
        curr = &ec.t_rows[ec.cy];

        curr->size = ec.cx;
        curr->content[curr->size] = '\0';
        update_text_row(curr);
    }

    ec.cy++;
    ec.cx = 0;
}

void delete_char(void)
{
    if (ec.cy == ec.num_trows)
        return;

    if (ec.cx == 0 && ec.cy == 0)
        return;

    text_row *action_row = &ec.t_rows[ec.cy];

    if (ec.cx > 0) {
        text_row_delete_char(action_row, ec.cx - 1);
        ec.cx--;
    } else {
        assert(ec.cx == 0);
        ec.cx = ec.t_rows[ec.cy - 1].size;
        text_row_append_string(&ec.t_rows[ec.cy - 1], action_row->content,
                               action_row->size);
        delete_text_row(ec.cy);
        ec.cy--;
    }
}

char *rows_to_string(int *buf_len)
{
    int total_len = 0;
    int i;

    for (i = 0; i < ec.num_trows; ++i) {
        // add one for the newline character
        total_len += ec.t_rows[i].size + 1;
    }

    *buf_len = total_len;

    char *buf = malloc(total_len);
    char *tmp = buf;

    for (i = 0; i < ec.num_trows; ++i) {
        memcpy(tmp, ec.t_rows[i].content, ec.t_rows[i].size);
        tmp += ec.t_rows[i].size;
        *tmp = '\n';
        tmp++;
    }

    return buf;
}

void save(void)
{
    if (ec.filename == NULL) {
        ec.filename = prompt("Save file as: %s", NULL);
        if (!ec.filename) {
            set_status_msg("Saving cancelled");
            return;
        }
        select_syntax_highlight();
    }

    int len;
    char *buf = rows_to_string(&len);

    // create file if it does not exist
    int fd = open(ec.filename, O_RDWR | O_CREAT,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); /* 0644 permissions */
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) {
            if (write(fd, buf, len) == len) {
                close(fd);
                FREE(buf);
                set_status_msg("\"%s\" %d Line%s, %d bytes written",
                               ec.filename, ec.num_trows,
                               ec.num_trows == 1 ? "" : "s", len);
                ec.dirty = 0;
                return;
            }
        }
        close(fd);
    }

    FREE(buf);
    set_status_msg("Cannot write: I/O error: %s", strerror(errno));
}

void handle_win_resize(int sig)
{
    if (get_window_size(&ec.rows, &ec.cols) == -1) {
        DIE("Could not get window size");
    }

    ec.rows -= 2;
    refresh_screen();
    signal(sig, handle_win_resize);
}
