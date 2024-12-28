#include <ctype.h>
#include <string.h>

#include "highlight.h"

static int is_separator(int c)
{
    return isspace(c) || c == '\0' || strchr("().,/+-=*~%<>[];", c) != NULL;
}

void update_syntax(text_row *tr)
{
    tr->highlight = realloc(tr->highlight, tr->render_size);
    memset(tr->highlight, HL_NORMAL, tr->render_size);

    int prev_separator = 1;
    int i = 0;
    while (i < tr->render_size) {
        unsigned char prev_highlight;
        if (i > 0) {
            prev_highlight = tr->highlight[i - 1];
        } else {
            prev_highlight = HL_NORMAL;
        }

        char c = tr->to_render[i];
        if ((isdigit(c) && (prev_separator || prev_highlight == HL_NUMBER)) ||
            (c == '.' && prev_highlight == HL_NUMBER)) {
            tr->highlight[i] = HL_NUMBER;
            i++;
            prev_separator = 0;
            continue;
        }

        prev_separator = is_separator(c);
        i++;
    }
}

int syntax_to_color(int highlight)
{
    switch (highlight) {
        case HL_NUMBER:
            return 31; // red
        case HL_MATCH:
            return 34; // blue
        default:
            return 37; // white
    }
}
