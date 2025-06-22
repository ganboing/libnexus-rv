// SPDX-License-Identifier: Apache 2.0
/*
 * hist_array.c - HIST array implementation
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <deque>
#include <cstdlib>
extern "C" {
#include <libnexus-rv/error.h>
#include <libnexus-rv/hist-array.h>
}

struct nexusrv_hist_array : std::deque<nexusrv_hist_arr_element> {};

extern "C" {

struct nexusrv_hist_array* nexusrv_hist_array_new(void) {
    __try {
        return new nexusrv_hist_array();
    } __catch(const std::bad_alloc&) {
        return nullptr;
    }
}

void nexusrv_hist_array_free(struct nexusrv_hist_array* arr) {
    delete arr;
}

nexusrv_hist_arr_element *nexusrv_hist_array_front(struct nexusrv_hist_array* arr) {
    return &arr->front();
}

void nexusrv_hist_array_pop(struct nexusrv_hist_array* arr) {
    arr->pop_front();
}

int nexusrv_hist_array_push(struct nexusrv_hist_array* arr, nexusrv_hist_arr_element ele) {
    __try {
        arr->push_back(ele);
        return 0;
    } __catch(const std::bad_alloc&) {
        return -nexus_no_mem;
    }
}

size_t nexusrv_hist_array_size(struct nexusrv_hist_array* arr) {
    return arr->size();
}

void nexusrv_hist_array_clear(struct nexusrv_hist_array* arr) {
    arr->clear();
}
}