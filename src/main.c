#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include "append_buffer.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define STEQS_VERSION "0.0.1"
#define STEQS_NAME "STEQS"

typedef struct {
    struct termios default_settings;
    int cx;   // cursor column position
    int cy;   // cursor row position
    int rows; // terminal window height
    int cols; // terminal window width
} editor_config;

enum keys {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN,
    HOME,
    END,
    DEL
};

editor_config ec;

/*  Terminal  */
void draw_row_tildes(abuf *buf)
{
    int i;
    for (i = 0; i < ec.rows; i++) {
        buf_append(buf, "~", 1);

        if (i == 1) {
            char welcome[30];
            int welcome_len =
                snprintf(welcome, sizeof(welcome), " %s - Version %s",
                         STEQS_NAME, STEQS_VERSION);
            int padding = (ec.cols - welcome_len - 1) / 2;

            while (padding > 0) {
                buf_append(buf, " ", 1);
                padding--;
            }

            buf_append(buf, welcome, welcome_len);
        }

        // erase from the active position to the end of line.
        // default param 0
        buf_append(buf, "\x1b[K", 3);

        if (i < ec.rows - 1) {
            buf_append(buf, "\r\n", 2);
        }
    }
}

void refresh_screen()
{
    abuf buf = ABUF_INIT;

    // Hide the cursor to get rid of flickering effect
    buf_append(&buf, "\x1b[?25l", 6);

    // Move cursor to the home position
    buf_append(&buf, "\x1b[H", 3);

    draw_row_tildes(&buf);

    // Move cursor to the home position
    char move_cmd[32];
    snprintf(move_cmd, sizeof(move_cmd), "\x1b[%d;%dH", ec.cy + 1, ec.cx + 1);
    buf_append(&buf, move_cmd, strlen(move_cmd));

    // Show the cursor
    buf_append(&buf, "\x1b[?25h", 6);

    write(STDOUT_FILENO, buf.buf, buf.len);
    buf_free(&buf);
}

void die(const char *s)
{
    refresh_screen();
    perror(s);
    exit(EXIT_FAILURE);
}

void disable_raw_mode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &ec.default_settings) == -1) {
        die("tcsetattr: Unable to set changed terminal settings");
    }
}

void enable_raw_mode()
{
    if (tcgetattr(STDIN_FILENO, &ec.default_settings) == -1) {
        die("tcgetattr: Unable to retrieve terminal settings");
    }
    atexit(disable_raw_mode);

    struct termios raw = ec.default_settings;
    cfmakeraw(&raw);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr: Unable to set changed terminal settings");
    }
}

int read_key()
{
    int read_res;
    char c;

    while ((read_res = read(STDIN_FILENO, &c, 1)) != 1) {
        if (read_res == -1 && errno != EAGAIN) {
            die("read: Unable to read input");
        }
    }

    if (c == '\x1b') {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1) {
            return '\x1b';
        }
        if (read(STDIN_FILENO, &seq[1], 1) != 1) {
            return '\x1b';
        }

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) {
                    return '\x1b';
                }
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1':
                            return HOME;
                        case '4':
                            return END;
                        case '3':
                            return DEL;
                        case '5':
                            return PAGE_UP;
                        case '6':
                            return PAGE_DOWN;
                        case '7':
                            return HOME;
                        case '8':
                            return END;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A':
                        return ARROW_UP;
                    case 'B':
                        return ARROW_DOWN;
                    case 'C':
                        return ARROW_RIGHT;
                    case 'D':
                        return ARROW_LEFT;
                    case 'H':
                        return HOME;
                    case 'F':
                        return END;
                }
            }
        } else {
            if (seq[0] == 'O') {
                switch (seq[1]) {
                    case 'H':
                        return HOME;
                    case 'F':
                        return END;
                }
            }
        }

        return '\x1b';
    } else {
        return c;
    }
}

void move_cursor(int key)
{
    switch (key) {
        case ARROW_UP:
            if (ec.cy > 0) {
                ec.cy--;
            }
            break;
        case ARROW_DOWN:
            if (ec.cy < ec.rows - 1) {
                ec.cy++;
            }
            break;
        case ARROW_LEFT:
            if (ec.cx > 0) {
                ec.cx--;
            }
            break;
        case ARROW_RIGHT:
            if (ec.cx < ec.cols - 1) {
                ec.cx++;
            }
            break;
    }
}

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

void init_editor()
{
    ec.cx = 0;
    ec.cy = 0;

    if (get_window_size(&ec.rows, &ec.cols) == -1) {
        die("Unable to get window size");
    }
}

int main()
{
    enable_raw_mode();
    init_editor();

    while (1) {
        refresh_screen();
        process_key();
    }

    return EXIT_SUCCESS;
}
