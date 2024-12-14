#include <ctype.h>
#include <string.h>

#include "highlight.h"

void update_syntax(text_row *tr)
{
    tr->highlight = realloc(tr->highlight, tr->render_size);
    memset(tr->highlight, HL_NORMAL, tr->render_size);

    int i;
    for (i = 0; i < tr->render_size; ++i) {
        if (isdigit(tr->to_render[i])) {
            tr->highlight[i] = HL_NUMBER;
        }
    }
}

int syntax_to_color(int highlight)
{
    switch (highlight) {
        case HL_NUMBER:
            return 31;
        default:
            return 37;
    }
}
