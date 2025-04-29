// SPDX-License-Identifer: Apache 2.0
/*
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_UTIL_MISC_H
#define LIBNEXUS_RV_UTIL_MISC_H

#include <unistd.h>
#include <stdint.h>

#ifdef __cplusplus
#include <memory>
#include <cstdio>
extern "C" {
#endif

ssize_t seek_pipe(int fd, size_t skip);

ssize_t seek_file(int fd, size_t offset);

int open_seek_file(char *filename, int oflags);

inline char base16(unsigned char c) {
    if (c < 10)
        return c + '0';
    return c + 'a' - 10;
}

inline void bin2hex(char *str, const uint8_t *bin, size_t n) {
    while (n--) {
        *str++ = base16(*bin / 16);
        *str++ = base16(*bin % 16);
        ++bin;
    }
}

#ifdef __cplusplus
}

struct auto_fd {
    int fd;
    inline auto_fd(int fd) : fd(fd) {}
    inline ~auto_fd() {
        if (fd >= 0)
            close(fd);
    }
};

typedef std::unique_ptr<FILE, decltype(&fclose)> auto_file;

#endif
#endif
