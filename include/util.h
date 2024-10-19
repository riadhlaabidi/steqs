#pragma once

#define CTRL_KEY(x) (x & 0x1f)

#define FREE(ptr)                                                              \
    do {                                                                       \
        free(ptr);                                                             \
        ptr = NULL;                                                            \
    } while (0)
