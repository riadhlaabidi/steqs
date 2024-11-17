#include <assert.h>
#include <ncurses.h>
#include <stdio.h>
#include <unistd.h>

#include "editor.h"

void move_up()
{
    if (ec.cy > 0) {
        ec.cy--;
    }
}

void move_down()
{
    if (ec.cy < ec.num_trows - 1) {
        ec.cy++;
    }
}

void move_left()
{

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
}

static void move_right()
{
    if (ec.cy < ec.num_trows) {
        text_row *row = &ec.t_rows[ec.cy];
        assert(row != NULL);
        if (ec.cx < row->size) {
            ec.cx++;
        } else {
            if (ec.cy < ec.num_trows - 1) {
                ec.cy++;
                ec.cx = 0;
            }
        }
    }
}

void move_cursor(int key)
{
    static void (*moves[])() = {[KEY_UP] = move_up,
                                [KEY_DOWN] = move_down,
                                [KEY_LEFT] = move_left,
                                [KEY_RIGHT] = move_right};

    moves[key]();

    // change cursor pos to the end of the current row
    // if the actual cursor pos is bigger than row's length.

    text_row *row = NULL;

    if (ec.cy < ec.num_trows) {
        row = &ec.t_rows[ec.cy];
    }

    int row_len = row ? row->size : 0;

    if (ec.cx > row_len) {
        ec.cx = row_len;
    }
}
