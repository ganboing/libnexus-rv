// SPDX-License-Identifer: Apache 2.0
/*
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <fcntl.h>
#include "misc.h"

// Read 1G maximum
#define MAX_READ_SIZE (1000UL * 1000UL * 1000UL)

ssize_t read_all(int fd, void *buf, size_t count) {
    void *orig_buf = buf;
    while (count) {
        size_t chunk = MAX_READ_SIZE;
        if (chunk > count)
            chunk = count;
        ssize_t ret = read(fd, buf, chunk); // Chunk != 0
        if (ret < 0)
            return ret;
        buf += ret;
        count -= ret;
        if ((size_t)ret < chunk)
            break; // Short read
    }
    return buf - orig_buf;
}