#pragma once

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
