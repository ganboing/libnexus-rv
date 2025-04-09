// SPDX-License-Identifer: Apache 2.0
/*
 * error.h - common error codes returned by decoder functions
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_ERROR_H
#define LIBNEXUS_RV_ERROR_H

enum nexus_error {
    nexus_ok,
    nexus_fail,
    nexus_buffer_too_small,
    nexus_stream_bad_mseo,
    nexus_stream_truncate,
    nexus_stream_read_failed,
    nexus_stream_write_failed,
    nexus_msg_invalid,
    nexus_msg_missing_field,
    nexus_msg_unsupported,
};

#endif
