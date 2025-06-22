// SPDX-License-Identifier: Apache 2.0
/*
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_LIB_MISC_H
#define LIBNEXUS_RV_LIB_MISC_H

#include <stdint.h>
#include <string.h>
#include <unistd.h>

ssize_t read_all(int fd, void *buf, size_t count);

#endif
