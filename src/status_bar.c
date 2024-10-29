#include "status_bar.h"
#include "append_buffer.h"
#include "editor.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void draw_status_bar(abuf *buf)
{
    // switch to inverted colors
    buf_append(buf, "\x1b[7m", 4);
    char status[80];
    char curr_line_status[80];

    int len = snprintf(status, sizeof(status), "%s%s",
                       ec.filename ? ec.filename : "[No name]",
                       ec.dirty ? "[+]" : "");
    int cl_len = snprintf(curr_line_status, sizeof(curr_line_status), "%d:%d ",
                          ec.cy + 1, ec.cx + 1);
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
