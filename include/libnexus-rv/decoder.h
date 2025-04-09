// SPDX-License-Identifer: Apache 2.0
/*
 * decoder.h - Nexus-RV decoder functions and declarations
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_DECODER_H
#define LIBNEXUS_RV_DECODER_H

#include "msg-types.h"
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

/** Nexus-RV Msg decoder configuration
 *
 * This should be set to match the HW implementation
 */
typedef struct nexusrv_dencoder_cfg {
    /* SRC bits */
    int src_bits;

    /* TIMESTAMP enabled */
    bool has_timestamp;
} nexusrv_dencoder_cfg;

/** Nexus-RV stream decoder context
 *
 * This should be initialized before calling nexusrv_decoder_iterate
 */
typedef struct nexusrv_decoder_context {
    /* Msg decoder configuration */
    nexusrv_dencoder_cfg decoder_cfg;

    /* File descriptor of binary trace file */
    int fd;

    /* Whether the trace file offset is synced
     *
     * if unset, nexusrv_sync_forward will be invoked
     * for the first call to nexusrv_decoder_iterate,
     * else, we assume file position of \@fd points to a full msg
     */
    bool synced;

    /* Buffer to hold chunks read from trace file
     *
     * It should be allocated by the user.
     */
    void *buffer;

    /* Buffer size
     *
     * It should be set by the user to indicate size of \@buffer
     */
    size_t bufsz;

    /* Currently filled bytes in \@buffer
     *
     * It should be initialized to 0 by the user and not changed directly.
     */
    size_t filled;

    /* Currently consumed bytes in \@buffer
     *
     * It should be initialized to 0 by the user and not changed directly.
     */
    size_t pos;
} nexusrv_decoder_context;

#define NEXUS_RV_MSG_MAX_BYTES 38

/** Sync forward for the first full msg. (following first byte of MSEO==3)
 * On success, returns the offset to the first full msg
 * The returned offset can be equal to \@limit.
 *
 * The \@limit argument should be set to the size of \@buffer
 *
 * Returns a positive integer on success, a negative error code otherwise
 *
 * Returns -nexus_stream_truncate if cannot find the first MSEO=3 byte
 */
ssize_t nexusrv_sync_forward(const uint8_t *buffer, size_t limit);

/** Sync backward for the last full msg (following last byte of MSEO==3)
 * On success, returns the offset to the last full msg
 * The returned offset can be equal to \@pos.
 *
 * The \@pos argument should be set to the current position in \@buffer
 *
 * Returns a positive integer on success, a negative error code otherwise
 *
 * Returns -nexus_stream_truncate if cannot find the last MSEO=3 byte
 */
ssize_t nexusrv_sync_backward(const uint8_t *buffer, size_t pos);

/** Decode the full Nexus-RV msg from \@buffer into \@msg.
 *
 * The \@cfg should match the HW implementation that produced the trace
 * The \@limit argument should be set to the number of bytes in \@buffer
 *
 * Returns the number of bytes consumed (a positive integer) on success,
 * a negative error code otherwise.
 *
 * Returns -nexus_stream_truncate if more bytes are expected from \@buffer
 * Returns -nexus_msg_invalid if decoded \@msg is invalid
 * Returns -nexus_msg_missing_field if decoded \@msg has missing fields
 */
ssize_t nexusrv_msg_decode(const nexusrv_dencoder_cfg *cfg,
                           const uint8_t *buffer, size_t limit,
                           nexusrv_msg *msg);

/** Iteratively decode all msg from trace file.
 *
 * It handles buffer fill and refill transparently, and caller is hence
 * freed from dealing with -nexus_stream_truncate.
 *
 * The \@context should be set according to the description above.
 * Memory allocation is done by the caller for maximum flexibility.
 *
 * Returns the number of bytes consumed (a positive integer) on a success
 * decoding of \@msg (same as nexusrv_msg_decode); 0 if no more msg to decode
 * (EOF); a negative error code on failure. \@context.pos is unchanged on failure.
 *
 * Returns -nexus_stream_truncate if EOF is already reached,
 *                                and there's a partial msg at the end.
 * Returns -nexus_stream_read_failed if read from \@context.fd has failed,
 *                                   specific error can be retrieved from errno
 * Returns -nexus_buffer_too_small if the size of \@context.bffer is too small
 *                                 to fit a single msg.
 * Returns -nexus_msg_invalid if decoded \@msg is invalid
 * Returns -nexus_msg_missing_field if decoded \@msg has missing fields
 */
ssize_t nexusrv_decoder_iterate(nexusrv_decoder_context *context,
                                nexusrv_msg *msg);

/** Print the \@msg in human readable form to \@fp
 *
 * Returns the total number of bytes written on success (as printf does).
 *
 * Returns the same return code of the first failed fprintf on failure.
 */
int nexusrv_print_msg(FILE *fp, const nexusrv_msg *msg);

#endif
