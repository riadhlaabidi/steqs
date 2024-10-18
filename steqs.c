#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#define CTRL_KEY(x) (x & 0x1f)

typedef struct {
    struct termios default_settings;
} editor_config;

editor_config ec;

/*  Terminal  */
void draw_row_tildes()
{
    int y;
    for (y = 0; y < 24; y++) {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}

void refresh_screen()
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    draw_row_tildes();
    write(STDOUT_FILENO, "\x1b[H", 3);
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

int main()
{
    enable_raw_mode();

    while (1) {
        refresh_screen();
        process_key();
    }

    return EXIT_SUCCESS;
}
