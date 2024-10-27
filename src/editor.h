#pragma once

#include "append_buffer.h"
#include <stdlib.h>
#include <termios.h>

#define EDITOR_VERSION "0.0.1"
#define EDITOR_NAME "STEQS"

#define TAB_STOP 8

typedef struct {
    int size;
    int render_size;
    char *content;
    char *to_render;
} text_row;

typedef struct {
    struct termios default_settings;
    int cx;   // cursor column position
    int cy;   // cursor row position
    int rx;   // cursor column position in rendered text
    int rows; // terminal window height
    int cols; // terminal window width
    int num_trows;
    text_row *t_rows;
    int row_offset;
    int col_offset;
    char *filename;
} editor_config;

extern editor_config ec;

void disable_raw_mode(void);

void enable_raw_mode(void);

void init_editor(void);

void refresh_screen(void);

void process_key(void);

int get_cursor_pos(int *rows, int *cols);

void move_cursor(int key);

int get_window_size(int *rows, int *cols);

void open_file(char *filename);

void append_text_row(char *content, size_t len);

void update_text_row(text_row *row);

void draw_row_tildes(abuf *buf);

void scroll(void);
