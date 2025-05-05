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
    nexus_no_mem,
    nexus_buffer_too_small,
    nexus_stream_bad_mseo,
    nexus_stream_truncate,
    nexus_stream_read_failed,
    nexus_msg_invalid,
    nexus_msg_missing_field,
    nexus_msg_unsupported,
    nexus_trace_eof,
    nexus_trace_not_synced,
    nexus_trace_hist_overflow,
    nexus_trace_icnt_overflow,
    nexus_trace_retstack_empty,
    nexus_trace_mismatch,
};

static inline const char *str_nexus_error(int err) {
    switch (err) {
        case nexus_ok:
            return "nexus_ok";
        case nexus_no_mem:
            return "nexus_no_mem";
        case nexus_buffer_too_small:
            return "nexus_buffer_too_small";
        case nexus_stream_bad_mseo:
            return "nexus_stream_bad_mseo";
        case nexus_stream_truncate:
            return "nexus_stream_truncate";
        case nexus_stream_read_failed:
            return "nexus_stream_read_failed";
        case nexus_msg_invalid:
            return "nexus_msg_invalid";
        case nexus_msg_missing_field:
            return "nexus_msg_missing_field";
        case nexus_msg_unsupported:
            return "nexus_msg_unsupported";
        case nexus_trace_eof:
            return "nexus_trace_eof";
        case nexus_trace_not_synced:
            return "nexus_trace_not_synced";
        case nexus_trace_hist_overflow:
            return "nexus_trace_hist_overflow";
        case nexus_trace_icnt_overflow:
            return "nexus_trace_icnt_overflow";
        case nexus_trace_retstack_empty:
            return "nexus_trace_retstack_empty";
        case nexus_trace_mismatch:
            return "nexus_trace_mismatch";
        default:
            return "(unknown)";
    }
}

#endif
