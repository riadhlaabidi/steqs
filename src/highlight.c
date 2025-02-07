#include <ctype.h>
#include <string.h>

#include "editor.h"
#include "highlight.h"

#define HIGHLIGHT_DB_ENTRIES (sizeof(HIGHLIGHT_DB) / sizeof(HIGHLIGHT_DB[0]))

char const *C_highlight_extensions[] = {".c", ".h", ".cpp", NULL};
char const *Go_highlight_extensions[] = {".go", NULL};

char const *C_highlight_keywords[] = {
    "switch", "case",     "default", "for",    "while",   "if",    "else",
    "break",  "continue", "return",  "struct", "typedef", "union", "static",
    "inline", "enum",     "class",   "int",    "float",   "long",  "double",
    "char",   "unsigned", "signed",  "void",   NULL};

char const *Go_highlight_keywords[] = {
    "break",  "default", "func",     "interface", "select",      "case",
    "defer",  "go",      "map",      "struct",    "chan",        "else",
    "goto",   "package", "switch",   "const",     "fallthrough", "if",
    "range",  "type",    "continue", "for",       "import",      "return",
    "var",    "any",     "bool",     "uint8",     "uint16",      "uint32",
    "uint64", "uint",    "uintptr",  "int8",      "int16",       "int32",
    "int64",  "int",     "float32",  "float64",   "complex64",   "complex128",
    "byte",   "rune",    NULL};

syntax HIGHLIGHT_DB[] = {{.file_type = "C",
                          .file_match = C_highlight_extensions,
                          .keywords = C_highlight_keywords,
                          .single_line_comment_start = "//",
                          .multiline_comment_start = "/*",
                          .multiline_comment_end = "*/",
                          .flags = HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},
                         {.file_type = "Go",
                          .file_match = Go_highlight_extensions,
                          .keywords = Go_highlight_keywords,
                          .single_line_comment_start = "//",
                          .multiline_comment_start = "/*",
                          .multiline_comment_end = "*/",
                          .flags = HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS}

};

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

    char const *scs = ec.syntax->single_line_comment_start;
    char const *mcs = ec.syntax->multiline_comment_start;
    char const *mce = ec.syntax->multiline_comment_end;

    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;

    int prev_separator = 1;
    int quote = 0;
    int multiline_comment =
        (tr->index > 0 && ec.t_rows[tr->index - 1].highlight_open_comment);

    int i = 0;
    while (i < tr->render_size) {
        unsigned char prev_highlight;

        if (i > 0) {
            prev_highlight = tr->highlight[i - 1];
        } else {
            prev_highlight = HL_NORMAL;
        }

        char c = tr->to_render[i];

        // Highlight single line comments except the ones that are inside
        // a string or a multiline comment
        if (scs_len && !quote && !multiline_comment) {
            if (!strncmp(&tr->to_render[i], scs, scs_len)) {
                memset(&tr->highlight[i], HL_COMMENT, tr->render_size - i);
                break;
            }
        }

        // Highlight multiline comments
        if (mcs_len && mce_len && !quote) {
            if (multiline_comment) {
                tr->highlight[i] = HL_COMMENT;
                if (strncmp(&tr->to_render[i], mce, mce_len) == 0) {
                    memset(&tr->highlight[i], HL_MULTILINE_COMMENT, mce_len);
                    i += mce_len;
                    multiline_comment = 0;
                    prev_separator = 1;
                    continue;
                } else {
                    i++;
                    continue;
                }
            } else if (strncmp(&tr->to_render[i], mcs, mcs_len) == 0) {
                memset(&tr->highlight[i], HL_MULTILINE_COMMENT, mcs_len);
                i += mcs_len;
                multiline_comment = 1;
                continue;
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

        char const **keywords = ec.syntax->keywords;
        if (prev_separator) {
            int j = 0;
            while (keywords[j]) {
                int keyword_len = strlen(keywords[j]);
                if (strncmp(&tr->to_render[i], keywords[j], keyword_len) == 0 &&
                    is_separator(tr->to_render[i + keyword_len])) {
                    memset(&tr->highlight[i], HL_KEYWORD, keyword_len);
                    i += keyword_len;
                    break;
                }
                j++;
            }
            if (keywords[j] != NULL) {
                prev_separator = 0;
                continue;
            }
        }

        prev_separator = is_separator(c);
        i++;
    }

    int changed = (tr->highlight_open_comment != multiline_comment);
    tr->highlight_open_comment = multiline_comment;
    if (changed && tr->index + 1 < ec.num_trows) {
        update_syntax(&ec.t_rows[tr->index + 1]);
    }
}

void select_syntax_highlight(void)
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
            j++;
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
        case HL_KEYWORD:
            return 33; // yellow
        case HL_MATCH:
            return 34; // blue
        case HL_COMMENT:
        case HL_MULTILINE_COMMENT:
            return 36; // cyan
        default:
            return 37; // white
    }
}
