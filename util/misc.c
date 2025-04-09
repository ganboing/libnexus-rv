// SPDX-License-Identifer: Apache 2.0
/*
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <fcntl.h>
#include "misc.h"

// Read 1G maximum
#define MAX_READ_SIZE (1000UL * 1000UL * 1000UL)

ssize_t seek_pipe(int fd, size_t skip) {
    int dev_null = open("/dev/null", O_WRONLY);
    if (dev_null < 0)
        return dev_null;
    size_t orig_skip = skip;
    while (skip) {
        size_t chunk = MAX_READ_SIZE;
        if (chunk > skip)
            chunk = skip;
        ssize_t ret = splice(fd, NULL, dev_null, NULL, chunk, 0);
        if (ret < 0)
            return ret;
        skip -= ret;
        if ((size_t)ret < chunk)
            break; // Short read
    }
    close(dev_null);
    return orig_skip - skip;
}