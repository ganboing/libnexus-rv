// SPDX-License-Identifier: Apache 2.0
/*
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <error.h>
#include <errno.h>
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
        if (ret < 0) {
            close(dev_null);
            return ret;
        }
        skip -= ret;
        if ((size_t)ret < chunk)
            break; // Short read
    }
    close(dev_null);
    return orig_skip - skip;
}

ssize_t seek_file(int fd, size_t offset) {
    ssize_t seeked = lseek(fd, offset, SEEK_SET);
    if (seeked >= 0 || errno != ESPIPE)
        return seeked;
    // Take care of unseekable pipe file
    return seek_pipe(fd, offset);
}

int open_seek_file(char *filename, int oflags) {
    size_t file_offset = 0;
    char *offset_str = strchr(filename, ':');
    if (offset_str != NULL) {
        file_offset = strtoul(offset_str, NULL, 0);
        *offset_str = '\0';
    }
    int fd = STDIN_FILENO;
    if (strcmp(filename, "-"))
        fd = open(filename, oflags);
    if (fd < 0)
        error(-1, errno, "Failed to open file %s", filename);
    if (seek_file(fd, file_offset) != (ssize_t)file_offset)
        error(-1, errno, "Failed to seek file %s", filename);
    return fd;
}