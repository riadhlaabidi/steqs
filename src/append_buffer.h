#ifndef INCLUDE_SRC_APPEND_BUFFER_H_
#define INCLUDE_SRC_APPEND_BUFFER_H_

#include <stdlib.h>

typedef struct {
    char *buf;
    size_t len;
} abuf;

#define ABUF_INIT {NULL, 0}

/**
 * Appends the string s to the end of the buffer.
 */
void buf_append(abuf *buf, const char *s, size_t len);

/**
 * Frees the allocated internal buffer
 */
void buf_free(abuf *bf);

#endif // INCLUDE_SRC_APPEND_BUFFER_H_
