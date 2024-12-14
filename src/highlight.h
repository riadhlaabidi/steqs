#pragma once

#include "editor.h"

enum highlight { HL_NORMAL = 0, HL_NUMBER };

void update_syntax(text_row *tr);

int syntax_to_color(int highlight);
