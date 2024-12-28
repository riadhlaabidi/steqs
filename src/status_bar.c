#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "append_buffer.h"
#include "editor.h"
#include "kbd.h"
#include "status_bar.h"
#include "util.h"

void draw_status_bar(abuf *buf)
{
    // switch to inverted colors
    buf_append(buf, "\x1b[7m", 4);
    char status[80];
    char curr_line_status[80];

    int len = snprintf(status, sizeof(status), "%s%s",
                       ec.filename ? ec.filename : "[No name]",
                       ec.dirty ? "[+]" : "");
    int cl_len =
        snprintf(curr_line_status, sizeof(curr_line_status), "%s | %d:%d ",
                 ec.syntax ? ec.syntax->file_type : "No file type", ec.cy + 1,
                 ec.cx + 1);
    if (len > ec.cols) {
        len = ec.cols;
    }

    buf_append(buf, status, len);

    while (len < ec.cols) {
        if (ec.cols - len == cl_len) {
            buf_append(buf, curr_line_status, cl_len);
            break;
        } else {
            buf_append(buf, " ", 1);
            len++;
        }
    }

    // switch back to normal formatting
    buf_append(buf, "\x1b[m", 3);

    // another line for status message
    buf_append(buf, "\r\n", 2);
}

void draw_message_bar(abuf *buf)
{
    // clear row
    buf_append(buf, "\x1b[K", 3);

    int msg_len = strlen(ec.status_msg);
    if (msg_len > ec.cols) {
        msg_len = ec.cols;
    }

    buf_append(buf, ec.status_msg, msg_len);
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

        int key = read_key();

        switch (key) {
            case DEL:
            case CTRL_KEY('h'):
            case BACKSPACE:
                if (buf_len > 0) {
                    buf[--buf_len] = '\0';
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

            case '\r':
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
