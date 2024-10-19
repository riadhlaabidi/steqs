#include "append_buffer.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void buf_append(abuf *buf, const char *s, size_t len)
{
    char *new = (char *)realloc(buf->buf, buf->len + len);

    if (new == NULL) {
        perror("Failed to reallocate buffer for appending");
        return;
    }

    memcpy(&new[buf->len], s, len);
    buf->buf = new;
    buf->len += len;
}

void buf_free(abuf *bf) { FREE(bf->buf); }
