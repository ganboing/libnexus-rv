// SPDX-License-Identifier: Apache 2.0
/*
 * msg-decoder.c - NexusRV message decoder implementation
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <libnexus-rv/error.h>
#include <libnexus-rv/msg-decoder.h>
#include <libnexus-rv/internal/protocol.h>
#include "misc.h"

#define MODEL_HWCFG_GENERIC32 "addr=32,maxstack=32"
#define MODEL_HWCFG_GENERIC64 "addr=64,maxstack=32"
#define MODEL_HWCFG_P550x4 "src=2,ts=40,addr=48,maxstack=1024,quirk-sifive"
#define MODEL_HWCFG_P550x8 "src=2,ts=40,addr=48,maxstack=1024,quirk-sifive"

static int __nexusrv_hwcfg_parse(nexusrv_hw_cfg *hwcfg,
                                 char *str) {
    char *sep = NULL;
    for (char *opt = str; opt; opt = sep) {
        sep = strchr(opt, ',');
        if (sep)
            *sep++ = '\0';
        if (!*opt)
            continue;
        bool negate = false;
        if (!strncmp(opt, "no-", 3)) {
            negate = true;
            opt += 3;
        }
        if (!strncmp(opt, "ts=", 3))
            hwcfg->ts_bits = atoi(opt + 3);
        else if (!strncmp(opt, "src=", 4))
            hwcfg->src_bits = atoi(opt + 4);
        else if (!strncmp(opt, "addr=", 5))
            hwcfg->addr_bits = atoi(opt + 5);
        else if (!strncmp(opt, "maxstack=", 9))
            hwcfg->max_stack = atoi(opt + 9);
        else if (!strcmp(opt, "quirk-sifive"))
            hwcfg->quirk_sifive = !negate;
        else if (!strncmp(opt, "timerfreq=", 10)) {
            char *end;
            hwcfg->timer_freq = strtoul(opt + 10, &end, 10);
            if (!hwcfg->timer_freq)
                return -nexus_hwcfg_invalid;
            if (!strcmp(end, "GHz"))
                hwcfg->timer_freq *= 1000ULL * 1000 * 1000;
            else if (!strcmp(end, "Mhz"))
                hwcfg->timer_freq *= 1000ULL * 1000;
            else if (!strcmp(end, "Khz"))
                hwcfg->timer_freq *= 1000;
            else if (strcmp(end, "Hz"))
                return -nexus_hwcfg_invalid;
        } else if (!strncmp(opt, "model=", 6)) {
            opt += 6;
            int rc = -nexus_hwcfg_invalid;
            if (!strcmp(opt, "generic32")) {
                char model_cfg[] = MODEL_HWCFG_GENERIC32;
                rc = __nexusrv_hwcfg_parse(hwcfg, model_cfg);
            } else if (!strcmp(opt, "generic64")) {
                char model_cfg[] = MODEL_HWCFG_GENERIC64;
                rc = __nexusrv_hwcfg_parse(hwcfg, model_cfg);
            } else if (!strcmp(opt, "p550x4")) {
                char model_cfg[] = MODEL_HWCFG_P550x4;
                rc = __nexusrv_hwcfg_parse(hwcfg, model_cfg);
            } else if (!strcmp(opt, "p550x8")) {
                char model_cfg[] = MODEL_HWCFG_P550x8;
                rc = __nexusrv_hwcfg_parse(hwcfg, model_cfg);
            }
            if (rc < 0)
                return rc;
        } else
            return -nexus_hwcfg_invalid;
    }
    return 0;
}

int nexusrv_hwcfg_parse(nexusrv_hw_cfg *hwcfg,
                        const char *str) {
    char cfg[strlen(str) + 1];
    strcpy(cfg, str);
    memset(hwcfg, 0, sizeof(*hwcfg));
    return __nexusrv_hwcfg_parse(hwcfg, cfg);
}

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
    if (bits < 64)
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

#define UNPACK_VAR_REQ(FIELD)                               \
({                                                          \
    size_t bits = UNPACK_VAR(FIELD);                        \
    if (!bits)                                              \
        return -nexus_msg_missing_field;                    \
    bits;                                                   \
})

#define UNPACK_XADDR_VAO(FIELD)                             \
do {                                                        \
    size_t bits = UNPACK_VAR_REQ(FIELD);                    \
    if (bits >= 64)                                         \
        break;                                              \
    if (FIELD & (1ULL << (bits - 1)))                       \
        FIELD |= (-1ULL << bits);                           \
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

ssize_t nexusrv_msg_decode(const struct nexusrv_hw_cfg *hwcfg,
                 const uint8_t *buffer, size_t limit, nexusrv_msg *msg) {
    bool eom = false;
    const uint8_t *orig_buffer = buffer;
    ssize_t consumed_bytes;
    size_t bit_offset;
    msg->timestamp = 0;
    CONSUME_BYTES();
    UNPACK_FIXED(NEXUS_RV_BITS_TCODE, msg->tcode);
    if (msg->tcode == NEXUSRV_TCODE_Idle) {
        if (!eom)
            return -nexus_msg_invalid;
        goto done;
    }
    msg->src = 0;
    if (hwcfg->src_bits)
        UNPACK_FIXED(hwcfg->src_bits, msg->src);
    switch (msg->tcode) {
        case NEXUSRV_TCODE_DirectBranch:
        case NEXUSRV_TCODE_DirectBranchSync:
        case NEXUSRV_TCODE_IndirectBranch:
        case NEXUSRV_TCODE_IndirectBranchSync:
        case NEXUSRV_TCODE_IndirectBranchHist:
        case NEXUSRV_TCODE_IndirectBranchHistSync:
        case NEXUSRV_TCODE_ProgTraceSync:
            break;
        default:
            goto handle_rest;
    }
    if (nexusrv_msg_is_sync(msg))
        UNPACK_FIXED(NEXUS_RV_BITS_ADDR_SYNC, msg->sync_type);
    if (nexusrv_msg_is_indir_branch(msg))
        UNPACK_FIXED(NEXUS_RV_BITS_ADDR_BTYPE, msg->branch_type);
    assert(nexusrv_msg_has_icnt(msg));
    msg->hrepeat = 0;
    UNPACK_VAR_REQ(msg->icnt);
    if (nexusrv_msg_has_xaddr(msg)) {
        CONSUME_BYTES();
        if (hwcfg->VAO)
            UNPACK_XADDR_VAO(msg->xaddr);
        else
            UNPACK_VAR_REQ(msg->xaddr);
    }
    if (nexusrv_msg_has_hist(msg)) {
        CONSUME_BYTES();
        UNPACK_VAR_REQ(msg->hist);
    }
    goto finished_common;
handle_rest:
    switch (msg->tcode) {
        default:
            while (!eom)
                CONSUME_BYTES();
            goto done;
        case NEXUSRV_TCODE_Ownership:
            UNPACK_FIXED(NEXUS_RV_BITS_OWNERSHIP_FMT, msg->ownership_fmt);
            UNPACK_FIXED(NEXUS_RV_BITS_OWNERSHIP_PRV, msg->ownership_priv);
            UNPACK_FIXED(NEXUS_RV_BITS_OWNERSHIP_V, msg->ownership_v);
            UNPACK_VAR(msg->context);
            break;
        case NEXUSRV_TCODE_Error:
            UNPACK_FIXED(NEXUS_RV_BITS_ETYPE, msg->error_type);
            UNPACK_VAR(msg->error_code);
            break;
        case NEXUSRV_TCODE_DataAcquisition:
            UNPACK_VAR_REQ(msg->idtag);
            CONSUME_BYTES();
            UNPACK_VAR_REQ(msg->dqdata);
            break;
        case NEXUSRV_TCODE_ResourceFull:
            UNPACK_FIXED(NEXUS_RV_BITS_RCODE, msg->res_code);
            if (msg->res_code > 2) {
                UNPACK_VAR(msg->res_data);
                break;
            }
            msg->icnt = 0;
            msg->hist = 0;
            msg->hrepeat = 0;
            switch (msg->res_code) {
                case 0:
                    UNPACK_VAR_REQ(msg->icnt);
                    break;
                case 1:
                    UNPACK_VAR_REQ(msg->hist);
                    break;
                case 2:
                    UNPACK_VAR_REQ(msg->hist);
                    CONSUME_BYTES();
                    UNPACK_VAR_REQ(msg->hrepeat);
                    break;
            }
            break;
        case NEXUSRV_TCODE_RepeatBranch:
            UNPACK_VAR_REQ(msg->hrepeat);
            break;
        case NEXUSRV_TCODE_ProgTraceCorrelation:
            msg->icnt = 0;
            msg->hist = 0;
            msg->hrepeat = 0;
            UNPACK_FIXED(NEXUS_RV_BITS_EVCODE, msg->stop_code);
            UNPACK_FIXED(NEXUS_RV_BITS_CDF, msg->cdf);
            UNPACK_VAR_REQ(msg->icnt);
            if (msg->cdf > 1)
                break;
            if (msg->cdf) {
                CONSUME_BYTES();
                UNPACK_VAR_REQ(msg->hist);
            }
            break;
    }
finished_common:
    if (hwcfg->ts_bits && eom) {
        if (nexusrv_msg_is_sync(msg))
            return -nexus_msg_missing_field;
        goto done;
    }
    while (!eom)
        CONSUME_BYTES();
    if (hwcfg->ts_bits)
        UNPACK_VAR_REQ(msg->timestamp);
done:
    return buffer - orig_buffer;
}

ssize_t nexusrv_msg_decoder_next(nexusrv_msg_decoder *decoder,
                                 nexusrv_msg *msg) {
    assert(decoder->pos <= decoder->filled);
    assert(decoder->filled <= decoder->bufsz);
    ssize_t carry, rc;
try_again:
    decoder->lastmsg_len = 0;
    if (decoder->pos == decoder->filled) {
        if (decoder->pos)
            // Already reached EOF
            return 0;
        goto read_buffer;
    }
    // We have some bytes to read in buffer
    rc = nexusrv_msg_decode(
            decoder->hw_cfg,
            decoder->buffer + decoder->pos,
            decoder->filled - decoder->pos, msg);
    if (rc >= 0) {
        decoder->pos += rc;
        // Reset the pos/filled to prepare for next iteration
        if (decoder->pos == decoder->bufsz) {
            decoder->nread += decoder->pos;
            decoder->pos = decoder->filled = 0;
        }
        decoder->lastmsg_len = rc;
        // Check SRC filter
        if (decoder->src_filter >=0 &&
            decoder->src_filter != msg->src)
            goto try_again;
        return rc;
    }
    // We have already reached EOF, so it's a real stream truncate
    if (decoder->filled != decoder->bufsz || rc != -nexus_stream_truncate)
        return rc;
    // We have read the full buffer, but still got stream truncate,
    // Buffer is too small
    if (!decoder->pos)
        return -nexus_buffer_too_small;
read_buffer:
    carry = decoder->filled - decoder->pos;
    memmove(decoder->buffer, decoder->buffer + decoder->bufsz - carry, carry);
    decoder->nread += decoder->pos;
    decoder->pos = 0;
    decoder->filled = carry;
    rc = read_all(decoder->fd, decoder->buffer + carry, decoder->bufsz - carry);
    if (rc < 0)
        return -nexus_stream_read_failed;
    decoder->filled += rc;
    if (decoder->filled > 0)
        // No need to fear for infinite recursion, as pos = 0 && filled > 0
        goto try_again;
    // read nothing and nothing left, set the pos/filled to max
    decoder->pos = decoder->filled = decoder->bufsz;
    return 0;
}

uint8_t *nexusrv_msg_decoder_lastmsg(nexusrv_msg_decoder *decoder) {
    assert(decoder->pos <= decoder->filled);
    assert(decoder->filled <= decoder->bufsz);
    if (!decoder->lastmsg_len)
        return NULL;
    assert(!decoder->pos || decoder->pos >= decoder->lastmsg_len);
    if (decoder->pos)
        return decoder->buffer + decoder->pos - decoder->lastmsg_len;
    else
        return decoder->buffer + decoder->bufsz - decoder->lastmsg_len;
}

void nexusrv_msg_decoder_rewind_last(nexusrv_msg_decoder *decoder) {
    assert(decoder->pos <= decoder->filled);
    assert(decoder->filled <= decoder->bufsz);
    if (!decoder->lastmsg_len)
        return;
    assert(!decoder->pos || decoder->pos >= decoder->lastmsg_len);
    if (!decoder->pos) {
        assert(!decoder->filled);
        assert(decoder->nread >= decoder->bufsz);
        decoder->pos = decoder->filled = decoder->bufsz;
        decoder->nread -= decoder->bufsz;
    }
    decoder->pos -= decoder->lastmsg_len;
    decoder->lastmsg_len = 0;
}