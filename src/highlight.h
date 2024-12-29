#ifndef INCLUDE_SRC_HIGHLIGHT_H_
#define INCLUDE_SRC_HIGHLIGHT_H_

#include "editor.h"

#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)

#define HIGHLIGHT_DB_ENTRIES (sizeof(HIGHLIGHT_DB) / sizeof(HIGHLIGHT_DB[0]))

enum highlight { HL_NORMAL = 0, HL_STRING, HL_NUMBER, HL_COMMENT, HL_MATCH };

void select_syntax_highlight();

void update_syntax(text_row *tr);

int syntax_to_color(int highlight);

#endif // INCLUDE_SRC_HIGHLIGHT_H_
