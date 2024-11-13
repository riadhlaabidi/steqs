#include <string.h>

#include "editor.h"
#include "find.h"
#include "status_bar.h"
#include "util.h"

void find()
{
    int prev_cx = ec.cx;
    int prev_cy = ec.cy;
    int prev_row_offset = ec.row_offset;
    int prev_col_offset = ec.col_offset;

    char *query = prompt("Search: %s", find_callback);

    if (query) {
        FREE(query);
    } else {
        // back to cursor position before initiating search
        ec.cx = prev_cx;
        ec.cy = prev_cy;
        ec.row_offset = prev_row_offset;
        ec.col_offset = prev_col_offset;
    }
}

void find_callback(char *query, int key)
{
    if (key == '\r' || key == '\x1b') {
        return;
    }

    int i;
    for (i = 0; i < ec.num_trows; ++i) {
        text_row *row = &ec.t_rows[i];
        char *match = strstr(row->to_render, query);
        if (match) {
            ec.cy = i;
            ec.cx = row_rx_to_cx(row, match - row->to_render);
            ec.row_offset = ec.num_trows;
            break;
        }
    }
}
