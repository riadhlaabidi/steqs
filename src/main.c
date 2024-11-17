#include <ncurses.h>

#include "editor.h"

int main(int argc, char *argv[])
{
    init_editor();

    if (argc >= 2) {
        open_file(argv[1]);
    }

    while (1) {
        refresh_screen();
        process_key();
    }

    return EXIT_SUCCESS;
}
