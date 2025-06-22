// SPDX-License-Identifier: Apache 2.0
/*
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_UTIL_MISC_H
#define LIBNEXUS_RV_UTIL_MISC_H

#include <unistd.h>
#include <stdint.h>

#ifdef __cplusplus
#include <memory>
#include <fstream>
#include <cstdio>
#include <cstdarg>
#include <cassert>
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
    inline void reset(int new_fd) {
        if (fd >= 0)
            close(fd);
        fd = new_fd;
    }
    inline ~auto_fd() {
        if (fd >= 0)
            close(fd);
    }
};

typedef std::unique_ptr<FILE, decltype(&fclose)> auto_file;

inline std::string
cppfmt(const char *fmt, ...) {
    std::string ret;
    va_list vl;
    va_start(vl, fmt);
    int len = vsnprintf(nullptr, 0, fmt, vl);
    va_end(vl);
    assert(len >= 0);
    if (!len)
        return ret;
    ret.resize(len);
    assert(!ret.empty());
    va_start(vl, fmt);
    vsprintf(&ret.front(), fmt, vl);
    va_end(vl);
    return ret;
}

inline std::string
read_file(const char *filename) {
    std::ifstream ifs(filename);
    return std::string(
        std::istreambuf_iterator<char>(ifs),
        std::istreambuf_iterator<char>());
}

inline std::vector<uint8_t>
read_file_binary(const char *filename) {
    std::ifstream ifs(filename, std::ios::in | std::ios::binary);
    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(ifs),
        std::istreambuf_iterator<char>());
}

#endif
#endif
