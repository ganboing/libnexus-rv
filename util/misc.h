// SPDX-License-Identifer: Apache 2.0
/*
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_UTIL_MISC_H
#define LIBNEXUS_RV_UTIL_MISC_H

#include <unistd.h>

ssize_t seek_pipe(int fd, size_t skip);

ssize_t seek_file(int fd, size_t offset);

int open_seek_file(char *filename, int oflags);

#endif
