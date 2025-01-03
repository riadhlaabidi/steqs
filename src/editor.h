#ifndef INCLUDE_SRC_EDITOR_H_
#define INCLUDE_SRC_EDITOR_H_

#include <stdlib.h>
#include <termios.h>

#include "append_buffer.h"

#define EDITOR_VERSION "0.0.1"
#define EDITOR_NAME "STEQS"
#define EDITOR_UNSAVED_QUIT_TIMES 2

#define TAB_STOP 8

typedef struct {
    int size;
    int render_size;
    char *content;
    char *to_render;
    unsigned char *highlight;
} text_row;

typedef struct {
    char const *file_type;
    char const **file_match;
    char const **keywords;
    char const *single_line_comment_start;
    int flags;
} syntax;

typedef struct {
    struct termios default_settings;
    syntax *syntax;
    int cx;   // cursor column position
    int cy;   // cursor row position
    int rx;   // cursor column position in the rendered text row
    int rows; // terminal window height
    int cols; // terminal window width
    int num_trows;
    text_row *t_rows;
    int row_offset;
    int col_offset;
    char *filename;
    char status_msg[96];
    int dirty;
    int prompting;
} editor_config;

extern editor_config ec;

void disable_raw_mode(void);

void enable_raw_mode(void);

void init_editor(void);

void refresh_screen(void);

void process_key(void);

int get_window_size(int *rows, int *cols);

void open_file(char *filename);

void insert_text_row(int pos, char *content, size_t len);

void delete_text_row(int pos);

void update_text_row(text_row *row);

void free_text_row(text_row *tr);

int row_rx_to_cx(text_row *tr, int rx);

void draw_row_tildes(abuf *buf);

void scroll(void);

void insert_char(int c);

void delete_char(void);

void insert_new_line(void);

void save(void);

void handle_win_resize(int sig);

#endif // INCLUDE_SRC_EDITOR_H_
