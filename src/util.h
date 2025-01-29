#ifndef INCLUDE_SRC_UTIL_H_
#define INCLUDE_SRC_UTIL_H_

#include "editor.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>

#define CTRL_KEY(x) (x & 0x1f)

#define FREE(ptr)                                                              \
    do {                                                                       \
        free(ptr);                                                             \
        ptr = NULL;                                                            \
    } while (0)

#define DIE(s)                                                                 \
    do {                                                                       \
        LOG(ERROR, s);                                                         \
        exit(EXIT_FAILURE);                                                    \
    } while (0)

static inline int count_digits(int number)
{
    int res = 0;
    while (number) {
        res++;
        number /= 10;
    }
    return res;
}

#endif // INCLUDE_SRC_UTIL_H_
