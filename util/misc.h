// SPDX-License-Identifer: Apache 2.0
/*
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_UTIL_MISC_H
#define LIBNEXUS_RV_UTIL_MISC_H

#include <unistd.h>

ssize_t readAll(int Fd, void *Buf, size_t Count);

ssize_t seekPipe(int Fd, size_t Skip);

#endif
