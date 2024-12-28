#ifndef INCLUDE_SRC_KBD_H_
#define INCLUDE_SRC_KBD_H_

enum keys {
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN,
    HOME,
    END,
    DEL
};

int read_key(void);

#endif // INCLUDE_SRC_KBD_H_
