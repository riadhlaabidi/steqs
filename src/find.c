#include <string.h>

#include "editor.h"
#include "status_bar.h"
#include "util.h"

void find()
{
    char *query = prompt("Search: %s");

    if (!query)
        return;

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

    FREE(query);
}
