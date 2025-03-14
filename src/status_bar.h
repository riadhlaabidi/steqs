#ifndef INCLUDE_SRC_STATUS_BAR_H_
#define INCLUDE_SRC_STATUS_BAR_H_

#include "append_buffer.h"

void draw_status_bar(abuf *buf);

void draw_message_bar(abuf *buf);

void set_status_msg(const char *fmt, ...);

/**
 * Initiates a prompt in the status bar and returns provided user input
 */
char *prompt(char *prompt, void (*callback)(char *, int));

#endif // INCLUDE_SRC_STATUS_BAR_H_
