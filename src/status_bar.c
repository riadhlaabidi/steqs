#include "status_bar.h"
#include "editor.h"
#include "util.h"

#include <ctype.h>
#include <ncurses.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void draw_status_bar()
{
    int i;

    char curr_line_status[80];
    int cl_len = snprintf(curr_line_status, sizeof(curr_line_status), "%d:%d",
                          ec.cy + 1, ec.cx + 1);

    char *filename = ec.filename ? ec.filename : "[No name]";
    char *dirty = ec.dirty ? "[+]" : "";

    move(ec.rows, 0);
    /*clrtoeol();*/
    attron(A_REVERSE);

    printw("%s%s", filename, dirty);

    if (cl_len > 0) {
        int pad_len = ec.cols - (strlen(filename) + strlen(dirty) + cl_len);
        for (i = 0; i < pad_len; ++i) {
            addch(' ');
        }
        printw("%s", curr_line_status);
    }

    attroff(A_REVERSE);

    move(ec.rows + 1, 0);
    clrtoeol();
    printw("%s", ec.status_msg);
}

void set_status_msg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ec.status_msg, sizeof(ec.status_msg), fmt, ap);
    va_end(ap);
}

char *prompt(char *prompt, void (*callback)(char *, int))
{
    size_t buf_size = 128;
    char *buf = malloc(buf_size);

    if (!buf) {
        DIE("Failed to allocate memory");
    }

    size_t buf_len = 0;
    buf[0] = '\0';

    ec.prompting = 1;

    while (ec.prompting) {
        set_status_msg(prompt, buf);
        refresh_screen();

        int key = getch();

        switch (key) {
            case CTRL_KEY('h'):
            case KEY_BACKSPACE:
            case KEY_DC:
                if (buf_len > 0) {
                    buf[--buf_len] = '\0';
                    delch();
                }

                if (callback)
                    callback(buf, key);
                break;

            case '\x1b': // cancelling
                set_status_msg("");
                if (callback)
                    callback(buf, key);
                FREE(buf);
                ec.prompting = 0;
                break;

            case CTRL_KEY('m'):
            case CTRL_KEY('j'):
            case KEY_ENTER: // Pressed Enter and gave a file name
                if (buf_len != 0) {
                    set_status_msg("");
                    ec.prompting = 0;
                    if (callback)
                        callback(buf, key);
                }
                break;

            default:
                if (!iscntrl(key) && key < 128) {
                    // NOTE: (SHADY) Make buf bigger if reached max size
                    if (buf_len == buf_size - 1) {
                        buf_size *= 2;
                        buf = realloc(buf, buf_size);
                    }
                    buf[buf_len++] = key;
                    buf[buf_len] = '\0';
                }

                if (callback)
                    callback(buf, key);
                break;
        }
    }

    return buf;
}
