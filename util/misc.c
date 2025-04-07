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

ssize_t readAll(int Fd, void *Buf, size_t Count) {
    void *OrigBuf = Buf;
    while (Count) {
        size_t Chunk = MAX_READ_SIZE;
        if (Chunk > Count)
            Chunk = Count;
        ssize_t Ret = read(Fd, Buf, Chunk); // Chunk != 0
        if (Ret < 0)
            return Ret;
        Buf += Ret;
        Count -= Ret;
        if ((size_t)Ret < Chunk)
            break; // Short read
    }
    return Buf - OrigBuf;
}

ssize_t seekPipe(int Fd, size_t Skip) {
    int DevNull = open("/dev/null", O_WRONLY);
    if (DevNull < 0)
        return DevNull;
    size_t OrigSkip = Skip;
    while (Skip) {
        size_t Chunk = MAX_READ_SIZE;
        if (Chunk > Skip)
            Chunk = Skip;
        ssize_t Ret = splice(Fd, NULL, DevNull, NULL, Chunk, 0);
        if (Ret < 0)
            return Ret;
        Skip -= Ret;
        if ((size_t)Ret < Chunk)
            break; // Short read
    }
    close(DevNull);
    return OrigSkip - Skip;
}