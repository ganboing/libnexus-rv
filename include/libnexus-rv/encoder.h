// SPDX-License-Identifer: Apache 2.0
/*
 * encoder.h - Nexus-RV encoder functions and declarations
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_ENCODER_H
#define LIBNEXUS_RV_ENCODER_H

#include "decoder.h"

/** Encode the \@msg into \@buffer
 *
 * The \@cfg specifies the desired HW implementation.
 * The \@limit argument should be set to the number of bytes in \@buffer.
 *
 * Returns the number of bytes produced (a positive integer) on success,
 * a negative error code otherwise.
 *
 * Returns -nexus_stream_truncate if more bytes are required from \@buffer
 * Returns -nexus_msg_unsupported if the \@msg type is not supported
 */
ssize_t nexusrv_msg_encode(const nexusrv_dencoder_cfg *cfg,
                           uint8_t *buffer, size_t limit,
                           const nexusrv_msg *msg);

/** Read the text (human readable) form of msg from \@fp into \@msg
 *
 * Returns 0 on success. A negative value on failure.
 *
 * Returns -nexus_msg_missing_field if read \@msg has missing fields
 * Returns -nexus_msg_unsupported if the \@msg type is not supported
 */
int nexusrv_read_msg(FILE *fp, nexusrv_msg *msg);

#endif
