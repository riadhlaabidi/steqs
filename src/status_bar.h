#pragma once

void draw_status_bar();

void set_status_msg(const char *fmt, ...);

/**
 * Initiates a prompt in the status bar and returns provided user input
 */
char *prompt(char *prompt, void (*callback)(char *, int));
