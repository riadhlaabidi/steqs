#include "kbd.h"
#include "util.h"

#include <errno.h>
#include <unistd.h>

int read_key()
{
    int read_res;
    char c;

    while ((read_res = read(STDIN_FILENO, &c, 1)) != 1) {
        if (read_res == -1 && errno != EAGAIN) {
            DIE("read: Unable to read input");
        }
    }

    if (c == '\x1b') {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1) {
            return '\x1b';
        }
        if (read(STDIN_FILENO, &seq[1], 1) != 1) {
            return '\x1b';
        }

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) {
                    return '\x1b';
                }
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1':
                            return HOME;
                        case '4':
                            return END;
                        case '3':
                            return DEL;
                        case '5':
                            return PAGE_UP;
                        case '6':
                            return PAGE_DOWN;
                        case '7':
                            return HOME;
                        case '8':
                            return END;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A':
                        return ARROW_UP;
                    case 'B':
                        return ARROW_DOWN;
                    case 'C':
                        return ARROW_RIGHT;
                    case 'D':
                        return ARROW_LEFT;
                    case 'H':
                        return HOME;
                    case 'F':
                        return END;
                }
            }
        } else {
            if (seq[0] == 'O') {
                switch (seq[1]) {
                    case 'H':
                        return HOME;
                    case 'F':
                        return END;
                }
            }
        }

        return '\x1b';
    } else {
        return c;
    }
}
