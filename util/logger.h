// SPDX-License-Identifier: Apache 2.0
/*
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_LOGGER_H
#define LIBNEXUS_LOGGER_H

#include <string>
#include <cstdio>

struct logger {
    inline logger(FILE *fp) : fp(fp), dirty(false), bufpos(0), repeated(0){}
    inline ~logger() {
        flush();
    }
    size_t format(const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
    size_t print(const char *str);
    size_t print(const char *str, size_t len);
    void newline();
    void flush();
private:
    void maybe_marker();
    FILE* fp;
    bool dirty;
    std::string buf;
    size_t bufpos;
    size_t repeated;
};

#endif
