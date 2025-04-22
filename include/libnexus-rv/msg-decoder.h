// SPDX-License-Identifer: Apache 2.0
/*
 * decoder.h - NexusRV decoder functions and declarations
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_MSG_DECODER_H
#define LIBNEXUS_RV_MSG_DECODER_H

#include "msg-types.h"
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
/** @file */

/** @brief NexusRV Message decoder configuration
 *
 * This should be set to match the HW implementation
 */
typedef struct nexusrv_hw_cfg {
    unsigned src_bits; /*!< SRC bits */
    unsigned ts_bits;  /*!< TIMESTAMP bits */
    bool HTM;          /*!< HTM enabled */
    bool VAO;          /*!< Virtual Addresses Optimization */
    bool ext_sifive;   /*!< Sifive Vendor Extension */
} nexusrv_hw_cfg;

/** @brief NexusRV Message decoder context
 *
 * This should be initialized by nexusrv_msg_decoder_init
 * before calling nexusrv_msg_next
 */
typedef struct nexusrv_msg_decoder {
    const nexusrv_hw_cfg *hw_cfg;
    /*!< Hardware/Implementation configuration */
    int fd; /*!< File descriptor of binary trace file */
    void *buffer;       /*!< Buffer to hold chunks read from trace file */
    size_t bufsz;       /*!< Buffer size */
    size_t nread;       /*!< Number of bytes read */
    size_t filled;      /*!< Currently filled bytes in buffer */
    size_t pos;         /*!< Currently consumed bytes in buffer */
    size_t lastmsg_len; /*!< Length of last message in buffer */
} nexusrv_msg_decoder;

/** Maximum standard Message size in raw bytes */
#define NEXUS_RV_MSG_MAX_BYTES 38

/** @brief Sync forward for the first full Message.
 *
 * It stops at the byte where the preceding byte has MSEO==3
 *
 * @param buffer The buffer that holds Messages
 * @param limit Should be set to the number of bytes in \p buffer
 * @retval >0:
 *   The offset to the first full Message on success.
 *   The returned offset can be equal to \p limit, which means the
 *   \p buffer contains a single Message, and we have just passed it.
 * @retval -nexus_stream_truncate:
 *   If cannot find the beginning of the full Message in buffer
 */
ssize_t nexusrv_sync_forward(const uint8_t *buffer, size_t limit);

/** @brief Sync backward for the last full Message.
 *
 * It stops at the byte where the preceding byte has MSEO==3
 *
 * @param buffer The buffer that holds Messages
 * @param pos Should be set to the current position in \p buffer
 * @retval >0:
 *   The position of the first full Message on success.
 *   The returned position can be equal to \p pos, which means the
 *   \p buffer contains a single Message, and we have just passed it.
 * @retval -nexus_stream_truncate:
 *   If cannot find the beginning of the full Message in buffer
 */
ssize_t nexusrv_sync_backward(const uint8_t *buffer, size_t pos);

/** @brief Decode the full NexusRV Message from \p buffer into \p msg.
 *
 * @param [in] hwcfg Must match the HW implementation that produced the trace
 * @param buffer The buffer that holds the Message
 * @param limit Should be set to the number of bytes in \p buffer
 * @param [out] msg Decoded Message
 *
 * @retval >0:
 *   the number of bytes consumed (a positive integer) on a success
 *   decoding of \p msg
 * @retval -nexus_stream_truncate: if more bytes are expected from \p buffer
 * @retval -nexus_msg_invalid: if decoded \p msg is invalid
 * @retval -nexus_msg_missing_field: if decoded \p msg has missing fields
 */
ssize_t nexusrv_msg_decode(const nexusrv_hw_cfg *hwcfg,
                           const uint8_t *buffer, size_t limit,
                           nexusrv_msg *msg);

/** @brief Initialize the Message decoder
 *
 * @param [in,out] decoder The decoder context
 * @param [in] hwcfg HW/Implementation configuration
 * @param fd file descriptor of the trace file
 * @param buffer Caller allocated buffer
 * @param bufsz Size of caller allocated buffer
 */
static inline void nexusrv_msg_decoder_init(nexusrv_msg_decoder *decoder,
                                            const nexusrv_hw_cfg *hwcfg,
                                            int fd,
                                            uint8_t *buffer,
                                            size_t bufsz) {
    memset(decoder, 0, sizeof(*decoder));
    decoder->hw_cfg = hwcfg;
    decoder->fd = fd;
    decoder->buffer = buffer;
    decoder->bufsz = bufsz;
}

/** @brief Get the current byte offset of the Message decoder
 *
 * @param [in] decoder The decoder context
 * @return The current byte offset
 */
static inline size_t nexusrv_msg_decoder_offset(nexusrv_msg_decoder *decoder) {
    size_t offset = decoder->nread;
    if (decoder->pos != decoder->bufsz)
        offset += decoder->pos;
    return offset - decoder->lastmsg_len;
}

/** @brief Iteratively decode the next Nexus Message from trace file.
 *
 * It handles buffer fill and refill transparently, and caller is hence
 * freed from dealing with -nexus_stream_truncate.
 *
 * @param [in] decoder Should be initialized by nexusrv_msg_decoder_init
 * @param [out] msg Decoded Message
 * @retval ==0: if no more Message to decode (EOF)
 * @retval >0:
 *   the number of bytes consumed (a positive integer) on a success
 *   decoding of \p msg (same as nexusrv_msg_decode)
 * @retval -nexus_stream_truncate:
 *   if EOF is already reached, and there's a partial Message at the end.
 * @retval -nexus_stream_read_failed:
 *   if read \p decoder.fd has failed, error can be retrieved from errno
 * @retval -nexus_buffer_too_small:
 *   if the size of \p decoder.buffer is too small to fit a single Message
 * @retval -nexus_msg_invalid: if decoded \p msg is invalid
 * @retval -nexus_msg_missing_field: if decoded \p msg has missing fields
 */
ssize_t nexusrv_msg_decoder_next(nexusrv_msg_decoder *decoder,
                                 nexusrv_msg *msg);

/** @brief Get the pointer to the last successfully decoded Message bytes
 *
 * Returns the pointer to bytes of last Message. It must be called after
 * a successful return of nexusrv_msg_decoder_next, otherwise it returns
 * NULL
 *
 * @param [in] decoder The same decoder used by nexusrv_msg_decoder_next
 * @return The pointer to the raw Message bytes
 */
uint8_t *nexusrv_msg_decoder_lastmsg(nexusrv_msg_decoder *decoder);

/** @brief Rewind the decoder to the beginning of last Message
 *
 * Rewind the decoder to point to the beginning of last Message, so it
 * can be decoded again. This effectively "returns" the Message to the
 * decoder. It does nothing if there's no previous invocation of
 * nexusrv_msg_decoder_next, or it has already been called for
 * the previous nexusrv_msg_decoder_next.
 *
 * @param [in] decoder The same decoder used by nexusrv_msg_decoder_next
 */

void nexusrv_msg_decoder_rewind_last(nexusrv_msg_decoder *decoder);

/** @brief Print the \p msg in human readable form to \p fp
 *
 * Returns the total number of bytes written on success (as printf does),
 * Or, on failure, the same return code of the first failed fprintf.
 */
int nexusrv_print_msg(FILE *fp, const nexusrv_msg *msg);

#endif
