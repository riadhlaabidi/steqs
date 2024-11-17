#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _POSIX_C_SOURCE >= 200809L
#define _GNU_SOURCE

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "cursor.h"
#include "editor.h"
#include "find.h"
#include "status_bar.h"
#include "util.h"

editor_config ec;
int quit_times = EDITOR_UNSAVED_QUIT_TIMES;

void init_editor(void)
{
    initscr();
    raw();                /* enable raw mode */
    noecho();             /* disable echoing user input */
    keypad(stdscr, TRUE); /* enable reading function keys F1..., arrows, etc */

    ec.cx = 0;
    ec.cy = 0;
    ec.rx = 0;
    ec.num_trows = 0;
    ec.t_rows = NULL;
    ec.row_offset = 0;
    ec.col_offset = 0;
    ec.filename = NULL;
    ec.status_msg[0] = '\0';
    ec.dirty = 0;
    ec.prompting = 0;

    getmaxyx(stdscr, ec.rows, ec.cols); /* get window size */

    // leave two lines for status bar
    ec.rows -= 2;

    set_status_msg(USAGE_STATUS_MSG);
}

void open_file(char *filename)
{
    FREE(ec.filename);
    ec.filename = strdup(filename);

    FILE *fp = fopen(filename, "r");

    if (!fp) {
        DIE("Failed to open file");
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t line_len;

    while ((line_len = getline(&line, &linecap, fp)) != -1) {
        while (line_len > 0 &&
               (line[line_len - 1] == '\n' || line[line_len - 1] == '\r')) {
            line_len--;
        }
        insert_text_row(ec.num_trows, line, line_len);
    }
    FREE(line);
    fclose(fp);
    ec.dirty = 0;
}

void insert_text_row(int pos, char *content, size_t len)
{
    if (pos < 0 || pos > ec.num_trows)
        return;

    ec.t_rows = realloc(ec.t_rows, sizeof(text_row) * (ec.num_trows + 1));

    memmove(&ec.t_rows[pos + 1], &ec.t_rows[pos],
            sizeof(text_row) * (ec.num_trows - pos));

    ec.t_rows[pos].size = len;
    ec.t_rows[pos].content = malloc(len + 1);

    memcpy(ec.t_rows[pos].content, content, len);

    ec.t_rows[pos].content[len] = '\0';
    ec.t_rows[pos].render_size = 0;
    ec.t_rows[pos].to_render = NULL;
    update_text_row(&ec.t_rows[pos]);

    ec.num_trows++;
    ec.dirty++;
}

void delete_text_row(int pos)
{
    if (pos < 0 || pos >= ec.num_trows)
        return;

    free_text_row(&ec.t_rows[pos]);

    memmove(&ec.t_rows[pos], &ec.t_rows[pos + 1],
            sizeof(text_row) * (ec.num_trows - pos - 1));

    ec.num_trows--;
    ec.dirty++;
}

void free_text_row(text_row *tr)
{
    FREE(tr->content);
    FREE(tr->to_render);
}

void update_text_row(text_row *row)
{
    int tabs = 0;
    int i;

    for (i = 0; i < row->size; i++) {
        if (row->content[i] == '\t') {
            tabs++;
        }
    }

    int new_size = row->size + tabs * (TAB_STOP - 1) + 1;
    FREE(row->to_render);
    row->to_render = malloc(new_size);

    if (!row->to_render) {
        DIE("Failed to allocate memory");
    }

    int idx = 0;
    for (i = 0; i < row->size; i++) {
        if (row->content[i] == '\t') {
            row->to_render[idx++] = ' ';
            while (idx % TAB_STOP != 0) {
                row->to_render[idx++] = ' ';
            }
        } else {
            row->to_render[idx++] = row->content[i];
        }
    }

    row->to_render[idx] = '\0';
    row->render_size = idx;
}

void draw_row_tildes()
{
    int i;

    erase();
    for (i = 0; i < ec.rows; i++) {
        int file_row = i + ec.row_offset;
        if (ec.num_trows > file_row) {
            mvprintw(i, 0, "%s", &ec.t_rows[file_row].to_render[ec.col_offset]);
        } else {
            mvprintw(i, 0, "~");
            if (ec.num_trows == 0) {
                char wlc[48];
                int wlc_len = snprintf(wlc, sizeof(wlc), "%s - Version %s",
                                       EDITOR_NAME, EDITOR_VERSION);
                if (wlc_len > 0) {
                    mvprintw(ec.rows / 2, (ec.cols - wlc_len) / 2, "%s", wlc);
                }
            }
        }
    }
}

/**
 * Converts cursor index from character-based into a rendering-based
 * index after rendering escape character '\t' in a text row
 */
int row_cx_to_rx(text_row *tr, int cx)
{
    int rx = 0;
    int i;

    for (i = 0; i < cx; i++) {
        if (tr->content[i] == '\t') {
            rx += (TAB_STOP - 1) - (rx % TAB_STOP);
        }
        rx++;
    }

    return rx;
}

/**
 * Converts cursor index from rendering-based into a character based index
 * reverting escape character '\t' rendering in a text row.
 */
int row_rx_to_cx(text_row *tr, int rx)
{
    int curr_rx = 0;
    int cx;

    for (cx = 0; cx < tr->size; cx++) {
        if (tr->content[cx] == '\t') {
            curr_rx += (TAB_STOP - 1) - (curr_rx % TAB_STOP);
        }

        curr_rx++;

        if (curr_rx > rx) {
            return cx;
        }
    }

    return cx;
}

void steqs_scroll(void)
{
    ec.rx = 0;

    if (ec.cy < ec.num_trows) {
        ec.rx = row_cx_to_rx(&ec.t_rows[ec.cy], ec.cx);
    }

    if (ec.cy < ec.row_offset) {
        ec.row_offset = ec.cy;
    }

    if (ec.cy >= ec.row_offset + ec.rows) {
        ec.row_offset = ec.cy - ec.rows + 1;
    }

    if (ec.rx < ec.col_offset) {
        ec.col_offset = ec.rx;
    }

    if (ec.rx >= ec.col_offset + ec.cols) {
        ec.col_offset = ec.rx - ec.cols + 1;
    }
}

void refresh_screen(void)
{
    steqs_scroll();

    draw_row_tildes();
    draw_status_bar();

    int r = (ec.cy - ec.row_offset);
    int c = (ec.rx - ec.col_offset);

    if (ec.prompting) {
        r = ec.rows + 2;
        c = strlen(ec.status_msg) + 1;
    }

    move(r, c);
    refresh();
}

void process_key(void)
{
    int c = getch();

    switch (c) {
        case CTRL_KEY('m'):
        case CTRL_KEY('j'):
        case KEY_ENTER:
            insert_new_line();
            break;
        case CTRL_KEY('q'):
            if (ec.dirty && quit_times > 0) {
                set_status_msg("Warning! File has unsaved changes. Press ^q %d "
                               "more time%s to quit without saving changes.",
                               quit_times, quit_times == 1 ? "" : "s");
                quit_times--;
                return;
            }
            endwin();
            exit(EXIT_SUCCESS);
            break;
        case CTRL_KEY('s'):
            save();
            break;

        case CTRL_KEY('f'):
            find();
            break;

        case KEY_UP:
        case KEY_DOWN:
        case KEY_LEFT:
        case KEY_RIGHT:
            move_cursor(c);
            break;
        /*case PAGE_UP:*/
        /*    {*/
        /*        ec.cy = ec.row_offset;*/
        /**/
        /*        int i = ec.rows;*/
        /*        while (i--) {*/
        /*            move_cursor(c == PAGE_UP ? ARROW_UP :
         * ARROW_DOWN);*/
        /*        }*/
        /*    }*/
        /*    break;*/
        /*case PAGE_DOWN:*/
        /*    {*/
        /*        ec.cy = ec.row_offset + ec.rows - 1;*/
        /**/
        /*        if (ec.cy > ec.num_trows - 1) {*/
        /*            ec.cy = ec.num_trows - 1;*/
        /*        }*/
        /**/
        /*        int i = ec.rows;*/
        /*        while (i--) {*/
        /*            move_cursor(c == PAGE_UP ? ARROW_UP :
         * ARROW_DOWN);*/
        /*        }*/
        /*    }*/
        /*    break;*/
        /*case HOME:*/
        /*    ec.cx = 0;*/
        /*    break;*/
        /*case END:*/
        /*    if (ec.cy < ec.num_trows) {*/
        /*        ec.cx = ec.t_rows[ec.cy].size - 1;*/
        /*    }*/
        /*    break;*/
        case KEY_BACKSPACE:
            delete_char();
            break;
        /*case DEL:*/
        /*    move_cursor(ARROW_RIGHT);*/
        /*    delete_char();*/
        /*    break;*/
        /*case CTRL_KEY('l'):*/
        /*case '\x1b':*/
        /*    break;*/
        default:
            insert_char(c);
            break;
    }

    quit_times = EDITOR_UNSAVED_QUIT_TIMES;
}

void text_row_insert_char(text_row *tr, int pos, int c)
{
    if (pos < 0 || pos > tr->size) {
        pos = tr->size;
    }

    tr->content = realloc(tr->content, tr->size + 2);
    memmove(&tr->content[pos + 1], &tr->content[pos], tr->size - pos + 1);
    tr->content[pos] = c;
    tr->size++;

    update_text_row(tr);
    ec.dirty++;
}

void text_row_delete_char(text_row *tr, int pos)
{
    if (pos < 0 || pos >= tr->size) {
        return;
    }
    memmove(&tr->content[pos], &tr->content[pos + 1], tr->size - pos);
    tr->size--;
    update_text_row(tr);
    ec.dirty++;
}

void text_row_append_string(text_row *tr, char *s, size_t len)
{
    tr->content = realloc(tr->content, tr->size + len + 1);
    memcpy(&tr->content[tr->size], s, len);
    tr->size += len;
    tr->content[tr->size] = '\0';
    update_text_row(tr);
    ec.dirty++;
}

void insert_char(int c)
{
    if (ec.cy == ec.num_trows) {
        insert_text_row(ec.num_trows, "", 0);
    }

    text_row_insert_char(&ec.t_rows[ec.cy], ec.cx, c);
    printw("%c", c);
    ec.cx++;
}

void insert_new_line(void)
{
    if (ec.cx == 0) {
        insert_text_row(ec.cy, "", 0);
    } else {
        text_row *curr = &ec.t_rows[ec.cy];
        insert_text_row(ec.cy + 1, &curr->content[ec.cx], curr->size - ec.cx);

        // insert_text_row reallocates the t_rows array, need to reassign
        // curr
        curr = &ec.t_rows[ec.cy];

        curr->size = ec.cx;
        curr->content[curr->size] = '\0';
        update_text_row(curr);
    }

    ec.cy++;
    ec.cx = 0;
}

void delete_char(void)
{
    /*if (ec.cy == ec.num_trows)*/
    /*    return;*/

    if (ec.cx == 0 && ec.cy == 0)
        return;

    text_row *action_row = &ec.t_rows[ec.cy];

    if (ec.cx > 0) {
        ec.cx--;
        text_row_delete_char(action_row, ec.cx);
        mvdelch(ec.cy, ec.cx);
    } else {
        assert(ec.cx == 0);
        ec.cx = ec.t_rows[ec.cy - 1].size;
        text_row_append_string(&ec.t_rows[ec.cy - 1], action_row->content,
                               action_row->size);
        delete_text_row(ec.cy);
        deleteln();
        ec.cy--;
    }
}

char *rows_to_string(int *buf_len)
{
    int total_len = 0;
    int i;

    for (i = 0; i < ec.num_trows; ++i) {
        // add one for the newline character
        total_len += ec.t_rows[i].size + 1;
    }

    *buf_len = total_len;

    char *buf = malloc(total_len);
    char *tmp = buf;

    for (i = 0; i < ec.num_trows; ++i) {
        memcpy(tmp, ec.t_rows[i].content, ec.t_rows[i].size);
        tmp += ec.t_rows[i].size;
        *tmp = '\n';
        tmp++;
    }

    return buf;
}

void save(void)
{
    if (ec.filename == NULL) {
        ec.filename = prompt("Save file as: %s", NULL);
        if (!ec.filename) {
            set_status_msg("Saving cancelled");
            return;
        }
    }

    int len;
    char *buf = rows_to_string(&len);

    // create file if it does not exist
    int fd = open(ec.filename, O_RDWR | O_CREAT,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); /* 0644 permissions */
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) {
            if (write(fd, buf, len) == len) {
                close(fd);
                FREE(buf);
                set_status_msg("\"%s\" %d Line%s, %d bytes written",
                               ec.filename, ec.num_trows,
                               ec.num_trows == 1 ? "" : "s", len);
                ec.dirty = 0;
                return;
            }
        }
        close(fd);
    }

    FREE(buf);
    set_status_msg("Cannot write: I/O error: %s", strerror(errno));
}
