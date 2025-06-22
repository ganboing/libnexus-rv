// SPDX-License-Identifier: Apache 2.0
/*
 * msg-encoder.h - NexusRV Message encoder functions and declarations
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_MSG_ENCODER_H
#define LIBNEXUS_RV_MSG_ENCODER_H

#include "msg-decoder.h"
/** @file */

/** @brief Encode the \p msg into \p buffer
 *
 * @param [in] hwcfg Specifies the desired HW implementation.
 * @param [in, out] buffer The buffer that will hold the Message
 * @param [in] limit Should be set to the number of bytes in \p buffer
 * @param [in] msg The Message to encode
 * @retval >0: The number of bytes produced on success
 * @retval -nexus_stream_truncate: if more bytes are required from \p buffer
 * @retval -nexus_msg_unsupported: if the \p msg type is not supported
 */
ssize_t nexusrv_msg_encode(const nexusrv_hw_cfg *hwcfg,
                           uint8_t *buffer, size_t limit,
                           const nexusrv_msg *msg);

/** @brief Read the text (human readable) form of Message
 * from \p fp into \p msg
 *
 * @param [in] fp FILE pointer
 * @param [out] msg Parsed Message
 * @retval 0: Success
 * @retval -nexus_msg_missing_field: if Message has missing fields
 * @retval -nexus_msg_unsupported: if Message type is not supported
 */
int nexusrv_read_msg(FILE *fp, nexusrv_msg *msg);

#endif
