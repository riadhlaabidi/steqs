#include "append_buffer.h"
#include "editor.h"

#include <stdio.h>

void draw_status_bar(abuf *buf)
{
    // switch to inverted colors
    buf_append(buf, "\x1b[7m", 4);
    char status[80];
    char curr_line_status[80];

    int len = snprintf(status, sizeof(status), "%.20s - %d line%c",
                       ec.filename ? ec.filename : "[No name]", ec.num_trows,
                       ec.num_trows == 1 ? '\0' : 's');
    int cl_len = snprintf(curr_line_status, sizeof(curr_line_status), "%d/%d",
                          ec.cy + 1, ec.num_trows);
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
}
