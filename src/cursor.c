#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "cursor.h"
#include "editor.h"
#include "kbd.h"

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
                assert(ec.cx == 0);
                ec.cy--;
                int rs = ec.t_rows[ec.cy].size;
                if (rs > 0) {
                    ec.cx = rs;
                }
            }
            break;
        case ARROW_RIGHT:
            if (row) {
                if (ec.cx < row->size) {
                    ec.cx++;
                } else {
                    if (ec.cy < ec.num_trows - 1) {
                        ec.cy++;
                        ec.cx = 0;
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

int get_cursor_pos(int *rows, int *cols)
{
    char buf[32];
    size_t i = 0;

    // Command for Device Status Report (DSR), reporting active position (6)
    // using Cursor Position Report (CPR) control sequence which is
    // sent from VT100 to the host, in the form "ESC[Pn;PnR"
    // first param Pn is the line, second is the column
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
        return -1;
    }

    while (i < sizeof(buf) - 1) {

        // read one character at a time from returned CPR "ESC[Pn;PnR"
        if (read(STDIN_FILENO, &buf[i], 1) == -1) {
            return -1;
        }

        // End of sequence, line and col are read
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
