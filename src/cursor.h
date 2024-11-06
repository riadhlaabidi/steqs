#pragma once

/**
 * Gets cursor position in the window given its rows and columns count.
 */
int get_cursor_pos(int *rows, int *cols);

/**
 * Moves the cursor according to the given arrow key.
 */
void move_cursor(int key);
