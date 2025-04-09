// SPDX-License-Identifer: Apache 2.0
/*
 * decoder.c - Nexus-RV decoder implementation
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <libnexus-rv/error.h>
#include <libnexus-rv/decoder.h>
#include <libnexus-rv/internal/protocol.h>
#include "misc.h"

ssize_t nexusrv_sync_forward(const uint8_t *buffer, size_t limit) {
    for (size_t consumed = 0; consumed < limit;) {
        nexusrv_msg_byte msg_byte = {buffer[consumed++]};
        if (msg_byte.mseo == 3)
            return consumed;
    }
    return -nexus_stream_truncate;
}

ssize_t nexusrv_sync_backward(const uint8_t *buffer, size_t pos) {
    for (size_t consumed = pos; consumed;) {
        nexusrv_msg_byte msg_byte = {buffer[--consumed]};
        if (msg_byte.mseo == 3)
            return consumed + 1;
    }
    return -nexus_stream_truncate;
}

static ssize_t consume_bytes(const uint8_t *buffer, size_t limit, bool *eom) {
    for (size_t consumed = 0; consumed < limit;) {
        nexusrv_msg_byte msg_byte = {buffer[consumed++]};
        if (msg_byte.mseo == 2) // MESO == 2, reserved
            return -nexus_stream_bad_mseo;
        if (!msg_byte.mseo) // Msg continues
            continue;
        *eom = msg_byte.mseo == 3; // End of Msg
        return consumed;
    }
    return -nexus_stream_truncate;
}

static uint64_t unpack_bits(const uint8_t *buffer, size_t bit_offset,
                             unsigned bits) {
    uint64_t value = 0;
    size_t start_byte = bit_offset / NEXUS_RV_MDO_BITS;
    size_t last_byte = (bit_offset + bits - 1) / NEXUS_RV_MDO_BITS;
    for (;; --last_byte) {
        nexusrv_msg_byte msg_byte = {buffer[last_byte]};
        if (start_byte != last_byte) {
            value <<= NEXUS_RV_MDO_BITS;
            value |= msg_byte.mdo;
            continue;
        }
        unsigned shift = bit_offset % NEXUS_RV_MDO_BITS;
        value <<= (NEXUS_RV_MDO_BITS - shift);
        value |= (msg_byte.mdo >> shift);
        break;
    }
    value &= -1ULL >> (64 - bits);
    return value;
}

#define UNPACK_FIXED(FIELD_BITS, FIELD)                     \
do {                                                        \
    size_t bits_left =                                      \
        consumed_bytes * NEXUS_RV_MDO_BITS - bit_offset;    \
    if (bits_left < FIELD_BITS)                             \
        return -nexus_msg_missing_field;                    \
    FIELD = unpack_bits(buffer - consumed_bytes,            \
                        bit_offset, FIELD_BITS);            \
    bit_offset += FIELD_BITS;                               \
} while(0)

#define UNPACK_VAR(FIELD)                                   \
({                                                          \
    size_t bits_left =                                      \
        consumed_bytes * NEXUS_RV_MDO_BITS - bit_offset;    \
    if (!bits_left)                                         \
        FIELD = 0;                                          \
    else                                                    \
        FIELD = unpack_bits(buffer - consumed_bytes,        \
                            bit_offset, bits_left);         \
    bit_offset += bits_left;                                \
    bits_left;                                              \
})

#define UNPACK_VAR_REQ(FIELD)               \
do {                                        \
    if (!UNPACK_VAR(FIELD))                 \
        return -nexus_msg_missing_field;    \
} while(0)

#define CONSUME_BYTES()                                     \
do {                                                        \
    if (eom)                                                \
        return -nexus_msg_missing_field;                    \
    consumed_bytes = consume_bytes(buffer, limit, &eom);    \
    if (consumed_bytes < 0)                                 \
        return consumed_bytes;                              \
    buffer += consumed_bytes;                               \
    limit -= consumed_bytes;                                \
    bit_offset = 0;                                         \
}                                                           \
while(0)

ssize_t nexusrv_msg_decode(const struct nexusrv_dencoder_cfg *cfg,
                 const uint8_t *buffer, size_t limit, nexusrv_msg *msg) {
    bool eom = false;
    const uint8_t *orig_buffer = buffer;
    ssize_t consumed_bytes;
    size_t bit_offset;
    unsigned cdf;
    CONSUME_BYTES();
    UNPACK_FIXED(NEXUS_RV_BITS_TCODE, msg->common.tcode);
    msg->common.source = 0;
    msg->common.timestamp = 0;
    if (msg->common.tcode != NEXUSRV_TCODE_IDLE && cfg->src_bits)
        UNPACK_FIXED(cfg->src_bits, msg->common.source);
    switch (msg->common.tcode) {
        case NENUSRV_TCODE_OWNERSHIP:
            UNPACK_FIXED(NEXUS_RV_BITS_OWNERSHIP_FMT, msg->ownership.format);
            UNPACK_FIXED(NEXUS_RV_BITS_OWNERSHIP_PRV, msg->ownership.prv);
            UNPACK_FIXED(NEXUS_RV_BITS_OWNERSHIP_V, msg->ownership.v);
            UNPACK_VAR(msg->ownership.context);
            break;
        case NEXUSRV_TCODE_DIRBR:
            UNPACK_VAR_REQ(msg->dirbr.icnt);
            break;
        case NEXUSRV_TCODE_INDIRBR:
            UNPACK_FIXED(NEXUS_RV_BITS_ADDR_BTYPE, msg->indirbr.btype);
            UNPACK_VAR_REQ(msg->indirbr.icnt);
            CONSUME_BYTES();
            UNPACK_VAR_REQ(msg->indirbr.uaddr);
            break;
        case NEXUSRV_TCODE_ERROR:
            UNPACK_FIXED(NEXUS_RV_BITS_ETYPE, msg->error.etype);
            UNPACK_VAR(msg->error.ecode);
            break;
        case NEXUSRV_TCODE_PROG_SYNC:
            UNPACK_FIXED(NEXUS_RV_BITS_ADDR_SYNC, msg->prog_sync.sync);
            UNPACK_VAR_REQ(msg->prog_sync.icnt);
            CONSUME_BYTES();
            UNPACK_VAR_REQ(msg->prog_sync.faddr);
            break;
        case NEXUSRV_TCODE_DIRBR_SYNC: // Same as NexusRvTcodeProgTraceSync
            UNPACK_FIXED(NEXUS_RV_BITS_ADDR_SYNC, msg->dirbr_sync.sync);
            UNPACK_VAR_REQ(msg->dirbr_sync.icnt);
            CONSUME_BYTES();
            UNPACK_VAR_REQ(msg->dirbr_sync.faddr);
            break;
        case NEXUSRV_TCODE_INDIRBR_SYNC:
            UNPACK_FIXED(NEXUS_RV_BITS_ADDR_SYNC, msg->indirbr_sync.sync);
            UNPACK_FIXED(NEXUS_RV_BITS_ADDR_BTYPE, msg->indirbr_sync.btype);
            UNPACK_VAR_REQ(msg->indirbr_sync.icnt);
            CONSUME_BYTES();
            UNPACK_VAR_REQ(msg->indirbr_sync.faddr);
            break;
        case NEXUSRV_TCODE_RESOURCE_FULL:
            UNPACK_FIXED(NEXUS_RV_BITS_RCODE, msg->resource_full.rcode);
            if (!msg->resource_full.rcode)
                UNPACK_VAR_REQ(msg->resource_full.icnt);
            else if (msg->resource_full.rcode > 2)
                UNPACK_VAR(msg->resource_full.rdata);
            else {
                UNPACK_VAR_REQ(msg->resource_full.hist);
                if (msg->resource_full.rcode == 2) {
                    CONSUME_BYTES();
                    UNPACK_VAR_REQ(msg->resource_full.hrepeat);
                }
            }
            break;
        case NEXUSRV_TCODE_INDIRBR_HIST:
            UNPACK_FIXED(NEXUS_RV_BITS_ADDR_BTYPE, msg->indirbr_hist.btype);
            UNPACK_VAR_REQ(msg->indirbr_hist.icnt);
            CONSUME_BYTES();
            UNPACK_VAR_REQ(msg->indirbr_hist.uaddr);
            CONSUME_BYTES();
            UNPACK_VAR_REQ(msg->indirbr_hist.hist);
            break;
        case NEXUSRV_TCODE_INDIRBR_HIST_SYNC:
            UNPACK_FIXED(NEXUS_RV_BITS_ADDR_SYNC, msg->indirbr_hist_sync.sync);
            UNPACK_FIXED(NEXUS_RV_BITS_ADDR_BTYPE, msg->indirbr_hist_sync.btype);
            UNPACK_VAR_REQ(msg->indirbr_hist_sync.icnt);
            CONSUME_BYTES();
            UNPACK_VAR_REQ(msg->indirbr_hist_sync.faddr);
            CONSUME_BYTES();
            UNPACK_VAR_REQ(msg->indirbr_hist_sync.hist);
            break;
        case NEXUSRV_TCODE_REPEAT_BR:
            UNPACK_VAR_REQ(msg->repeat_br.hrepeat);
            break;
        case NEXUSRV_TCODE_PROG_CORR:
            UNPACK_FIXED(NEXUS_RV_BITS_EVCODE, msg->prog_corr.evcode);
            UNPACK_FIXED(NEXUS_RV_BITS_CDF, cdf);
            UNPACK_VAR_REQ(msg->prog_corr.icnt);
            msg->prog_corr.hist = 0;
            if (cdf == 1) {
                CONSUME_BYTES();
                UNPACK_VAR_REQ(msg->prog_corr.hist);
            }
            break;
        case NEXUSRV_TCODE_IDLE: // No timestamp
            if (!eom)
                return -nexus_msg_invalid;
            return buffer - orig_buffer;
    }
    if (cfg->has_timestamp && eom)
        return -nexus_msg_missing_field;
    while (!eom)
        CONSUME_BYTES();
    if (cfg->has_timestamp)
        UNPACK_VAR_REQ(msg->common.timestamp);
    return buffer - orig_buffer;
}

ssize_t nexusrv_decoder_iterate(nexusrv_decoder_context *ctx,
                                nexusrv_msg *msg) {
    assert(ctx->pos <= ctx->filled);
    assert(ctx->filled <= ctx->bufsz);
    ssize_t rc;
    if (ctx->pos == ctx->filled) {
        if (ctx->pos)
            // Already reached EOF
            return 0;
        goto read_buffer;
    }
    // We have some bytes to read in buffer
    if (ctx->synced)
        rc = nexusrv_msg_decode(
                &ctx->decoder_cfg,
                ctx->buffer + ctx->pos,
                ctx->filled - ctx->pos, msg);
    else
        rc = nexusrv_sync_forward(
                ctx->buffer + ctx->pos,
                ctx->filled - ctx->pos
        );
    if (rc >= 0) {
        ctx->pos += rc;
        // Reset the pos/filled to prepare for next iteration
        if (ctx->pos == ctx->bufsz)
            ctx->pos = ctx->filled = 0;
        if (ctx->synced)
            return rc;
        ctx->synced = true;
        return nexusrv_decoder_iterate(ctx, msg);
    }
    // We have already reached EOF, so it's a real stream truncate
    if (ctx->filled != ctx->bufsz || rc != -nexus_stream_truncate)
        return rc;
    // We have read the full buffer, but still got stream truncate,
    // Buffer is too small
    if (!ctx->pos)
        return -nexus_buffer_too_small;
read_buffer:
    size_t carry = ctx->filled - ctx->pos;
    memmove(ctx->buffer, ctx->buffer + ctx->bufsz - carry, carry);
    ctx->pos = 0;
    ctx->filled = carry;
    rc = read_all(ctx->fd, ctx->buffer + carry, ctx->bufsz - carry);
    if (rc < 0)
        return -nexus_stream_read_failed;
    ctx->filled += rc;
    if (ctx->filled > 0)
        // No need to fear for infinite recursion, as pos = 0 && filled > 0
        return nexusrv_decoder_iterate(ctx, msg);
    // read nothing and nothing left, set the pos/filled to max
    ctx->pos = ctx->filled = ctx->bufsz;
    return 0;
}