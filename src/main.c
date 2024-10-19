#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include "append_buffer.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

typedef struct {
    struct termios default_settings;
    int rows;
    int cols;
} editor_config;

editor_config ec;

/*  Terminal  */
void draw_row_tildes(abuf *buf)
{
    int i;
    for (i = 0; i < ec.rows; i++) {
        buf_append(buf, "~", 1);

        if (i < ec.rows - 1) {
            buf_append(buf, "\r\n", 2);
        }
    }
}

void refresh_screen()
{
    abuf buf = ABUF_INIT;
    buf_append(&buf, "\x1b[2J", 4);
    buf_append(&buf, "\x1b[H", 3);
    draw_row_tildes(&buf);
    buf_append(&buf, "\x1b[H", 3);

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

char read_key()
{
    int read_res;
    char c;

    while ((read_res = read(STDIN_FILENO, &c, 1)) != 1) {
        if (read_res == -1 && errno != EAGAIN) {
            die("read: Unable to read input");
        }
    }

    return c;
}

void process_key()
{
    char c = read_key();

    switch (c) {
    case CTRL_KEY('q'):
        exit(EXIT_SUCCESS);
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
