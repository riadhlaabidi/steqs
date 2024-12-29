#include <ctype.h>
#include <string.h>

#include "editor.h"
#include "highlight.h"

char const *C_highlight_extensions[] = {".c", ".h", ".cpp", NULL};

syntax HIGHLIGHT_DB[] = {{"c", C_highlight_extensions, "//",
                          HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS}};

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

    char const *cmt_start = ec.syntax->single_line_comment_start;
    int cmt_start_len = cmt_start ? strlen(cmt_start) : 0;

    int prev_separator = 1;
    int quote = 0;
    int i = 0;

    while (i < tr->render_size) {
        unsigned char prev_highlight;

        if (i > 0) {
            prev_highlight = tr->highlight[i - 1];
        } else {
            prev_highlight = HL_NORMAL;
        }

        char c = tr->to_render[i];

        if (cmt_start_len && !quote) {
            if (!strncmp(&tr->to_render[i], cmt_start, cmt_start_len)) {
                memset(&tr->highlight[i], HL_COMMENT, tr->render_size - i);
                break;
            }
        }

        if (ec.syntax->flags & HL_HIGHLIGHT_STRINGS) {
            if (quote) { // we're still inside a string
                tr->highlight[i] = HL_STRING;
                if (c == quote) { // matching second quote
                    if (i - 1 > 0 && tr->to_render[i - 1] != '\\') {
                        // if this is not an escaped quote, end the string
                        quote = 0;
                        prev_separator = 1;
                    }
                }
                i++;
                continue;
            } else {
                if (c == '"' || c == '\'') {
                    quote = c; // start of a string
                    tr->highlight[i] = HL_STRING;
                    i++;
                    continue;
                }
            }
        }

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
        case HL_STRING:
            return 32; // green
        case HL_MATCH:
            return 34; // blue
        case HL_COMMENT:
            return 36; // cyan
        default:
            return 37; // white
    }
}
