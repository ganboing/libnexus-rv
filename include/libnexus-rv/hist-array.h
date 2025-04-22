// SPDX-License-Identifer: Apache 2.0
/*
 * hist-array.h - HIST array structure and functions
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_HIST_ARRAY_H
#define LIBNEXUS_RV_HIST_ARRAY_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
/** @file */

/** @brief HIST array element
 *
 * If HIST is zero, this is a placeholder for the I-CNT Message
 * to be retired. It's used to keep track of delta timestamps
 */
typedef struct nexusrv_hist_arr_element {
    uint32_t hist;       /*!< HIST */
    uint32_t hrepeat;    /*!< HREPEAT */
    uint64_t timestamp;  /*!< TIMESTAMP (delta) */
} nexusrv_hist_arr_element;

struct nexusrv_hist_array;

struct nexusrv_hist_array* nexusrv_hist_array_new(void);

void nexusrv_hist_array_free(struct nexusrv_hist_array* arr);

nexusrv_hist_arr_element *nexusrv_hist_array_front(struct nexusrv_hist_array* arr);

void nexusrv_hist_array_pop(struct nexusrv_hist_array* arr);

int nexusrv_hist_array_push(struct nexusrv_hist_array* arr, nexusrv_hist_arr_element ele);

size_t nexusrv_hist_array_size(struct nexusrv_hist_array* arr);

void nexusrv_hist_array_clear(struct nexusrv_hist_array* arr);

#endif