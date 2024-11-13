#pragma once

/**
 * Launches a prompt for searching the currently opened file
 */
void find(void);

/**
 * Callback function to search for query match and set the cursor position to
 * the first result
 */
void find_callback(char *query, int key);
