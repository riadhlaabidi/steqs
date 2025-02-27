#include <string.h>

#include "editor.h"
#include "find.h"
#include "highlight.h"
#include "kbd.h"
#include "status_bar.h"
#include "util.h"

static int cx_before_find;
static int cy_before_find;
static int r_offset_before_find;
static int c_offset_before_find;

void restore_cursor_pos(void)
{

    ec.cy = cy_before_find;
    ec.cx = cx_before_find;
    ec.row_offset = r_offset_before_find;
    ec.col_offset = c_offset_before_find;
}

void find_callback(char *query, int key)
{
    static int last_match = -1;
    static int direction = 1;
    static int matches = 0;

    static int previous_hl_line;
    static char *previous_hl = NULL;

    if (previous_hl) {
        memcpy(ec.t_rows[previous_hl_line].highlight, previous_hl,
               ec.t_rows[previous_hl_line].render_size);
        FREE(previous_hl);
    }

    if (key == '\r' || key == '\x1b') {
        last_match = -1;
        direction = 1;
        if (!matches && key == '\r') {
            set_status_msg("No matches found!");
        }
        return;
    } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
        direction = 1;
    } else if (key == ARROW_LEFT || key == ARROW_UP) {
        direction = -1;
    } else {
        last_match = -1;
        direction = 1;
    }

    if (last_match == -1) {
        direction = 1;
    }

    int current = last_match;
    int i;
    matches = 0;

    for (i = 0; i < ec.num_trows; ++i) {
        current += direction;

        if (current == -1) {
            current = ec.num_trows - 1;
        } else if (current == ec.num_trows) {
            current = 0;
        }

        text_row *row = &ec.t_rows[current];
        char *match = strstr(row->to_render, query);

        if (match) {
            matches++;
            last_match = current;
            ec.cy = current;
            ec.cx = row_rx_to_cx(row, match - row->to_render);
            ec.row_offset = ec.num_trows;

            previous_hl_line = current;
            previous_hl = malloc(row->render_size);
            memcpy(previous_hl, row->highlight, row->render_size);
            memset(&row->highlight[match - row->to_render], HL_MATCH,
                   strlen(query));
            break;
        }
    }

    if (!matches) {
        restore_cursor_pos();
    }
}

void find(void)
{
    cx_before_find = ec.cx;
    cy_before_find = ec.cy;
    r_offset_before_find = ec.row_offset;
    c_offset_before_find = ec.col_offset;

    char *query = prompt(
        "Search [ESC: cancel, Arrows: next/prev match, Enter: select): %s",
        find_callback);

    if (query) {
        FREE(query);
    } else {
        restore_cursor_pos();
    }
}
