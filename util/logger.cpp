// SPDX-License-Identifier: Apache 2.0
/*
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <cstring>
#include <cassert>
#include <cstdarg>
#include "logger.h"

using namespace std;

void logger::maybe_marker() {
    if (!repeated)
        return;
    fprintf(fp, "<repeated %zu times>\n", repeated);
    repeated = 0;
}

size_t logger::print(const char *str, size_t len) {
    assert(bufpos <= buf.size());
    size_t left = buf.size() - bufpos;
    if (left >= len &&
        !strncmp(str, buf.c_str() + bufpos, len)) {
        // Repeating previous lines
        goto done;
    }
    dirty = true;
    maybe_marker();
    if (buf.size() < bufpos + len)
        buf.resize(bufpos + len);
    memcpy(buf.data() + bufpos, str, len);
done:
    bufpos += len;
    return len;
}

size_t logger::print(const char *str) {
    return print(str, strlen(str));
}

size_t logger::format(const char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);
    int len = vsnprintf(nullptr, 0, fmt, vl);
    char localbuf[len + 1];
    va_end(vl);
    va_start(vl, fmt);
    vsprintf(localbuf, fmt, vl);
    va_end(vl);
    return print(localbuf, len);
}

void logger::newline() {
    buf.resize(bufpos);
    bufpos = 0;
    if (dirty) {
        maybe_marker();
        fprintf(fp, "%s\n", buf.c_str());
    } else if (!buf.empty())
        ++repeated;
    dirty = false;
}

void logger::flush() {
    newline();
    buf.clear();
    maybe_marker();
}