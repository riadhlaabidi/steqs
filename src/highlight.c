#include <ctype.h>
#include <string.h>

#include "editor.h"
#include "highlight.h"

char const *C_highlight_extensions[] = {".c", ".h", ".cpp", NULL};

syntax HIGHLIGHT_DB[] = {{"c", C_highlight_extensions, HL_HIGHLIGHT_NUMBERS}};

static int is_separator(int c)
{
    return isspace(c) || c == '\0' || strchr("().,/+-=*~%<>[];", c) != NULL;
}

void update_syntax(text_row *tr)
{
    tr->highlight = realloc(tr->highlight, tr->render_size);
    memset(tr->highlight, HL_NORMAL, tr->render_size);

    // no file type
    if (ec.syntax == NULL) {
        return;
    }

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

        if (ec.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
            if ((isdigit(c) &&
                 (prev_separator || prev_highlight == HL_NUMBER)) ||
                (c == '.' && prev_highlight == HL_NUMBER)) {
                tr->highlight[i] = HL_NUMBER;
                i++;
                prev_separator = 0;
                continue;
            }
        }

        prev_separator = is_separator(c);
        i++;
    }
}

void select_syntax_highlight()
{
    ec.syntax = NULL;

    if (!ec.filename) {
        return;
    }

    char *extension = strrchr(ec.filename, '.');

    if (!extension) {
        return;
    }

    unsigned int i;
    for (i = 0; i < HIGHLIGHT_DB_ENTRIES; ++i) {
        syntax *s = &HIGHLIGHT_DB[i];
        unsigned int j = 0;
        while (s->file_match[j]) {
            if (strcmp(extension, s->file_match[j]) == 0) {
                ec.syntax = s;

                int row;
                for (row = 0; row < ec.num_trows; ++row) {
                    update_syntax(&ec.t_rows[row]);
                }

                return;
            }
            i++;
        }
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
