// SPDX-License-Identifer: Apache 2.0
/*
 * encoder.c - Nexus-RV encoder implementation
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <libnexus-rv/error.h>
#include <libnexus-rv/encoder.h>
#include <libnexus-rv/internal/protocol.h>

static void pack_bits(uint8_t *buffer, size_t bit_offset,
                         uint64_t value, unsigned bits) {
    if (!bits)
        return;
    value &= -1ULL >> (64 - bits);
    for (;;) {
        size_t byte = bit_offset / NEXUS_RV_MDO_BITS;
        nexusrv_msg_byte msg_byte = {buffer[byte]};
        unsigned shift = bit_offset % NEXUS_RV_MDO_BITS;
        unsigned available = NEXUS_RV_MDO_BITS - shift;
        if (!shift) {
            msg_byte.mseo = 0;
            msg_byte.mdo = value;
        }
        msg_byte.mdo |= (value << shift);
        buffer[byte] = msg_byte.byte;
        if (available > bits)
            break;
        value >>= available;
        bits -= available;
        bit_offset += available;
    }
}

#define PACK_FIELD(FIELD_BITS, FIELD)                           \
do {                                                            \
    if (bit_offset + FIELD_BITS > limit * NEXUS_RV_MDO_BITS)    \
        return -nexus_stream_truncate;                          \
    pack_bits(buffer, bit_offset, FIELD, FIELD_BITS);           \
    bit_offset += FIELD_BITS;                                   \
} while(0)

#define END_FIELD(MSEO)                             \
do {                                                \
    size_t byte;                                    \
    bit_offset += NEXUS_RV_MDO_BITS - 1;            \
    byte = bit_offset / NEXUS_RV_MDO_BITS;          \
    bit_offset = byte * NEXUS_RV_MDO_BITS;          \
    nexusrv_msg_byte msg_byte = {buffer[--byte]};   \
    msg_byte.mseo = MSEO;                           \
    buffer[byte] = msg_byte.byte;                   \
} while(0)

#define PACK_FIXED(FIELD_BITS, FIELD) PACK_FIELD(FIELD_BITS, FIELD)

#if !__has_builtin(__builtin_clzg)
#define __builtin_clzg(arg, prec)                       \
({                                                      \
    typeof(arg) v = (arg);                              \
    int extended = 8 *                                  \
        (sizeof(long) - sizeof (v));                    \
    !v ? prec : v < 0 ? __builtin_clzl((long)v) :       \
        __builtin_clzl((unsigned long)v) - extended;    \
})
#endif

#define PACK_VAR(FIELD)                                 \
do {                                                    \
    uint64_t value = FIELD;                             \
    PACK_FIELD(64 - __builtin_clzg(value, 64), value);  \
    END_FIELD(1);                                       \
} while(0)

#define PACK_VAR_REQ(FIELD)                             \
do {                                                    \
    uint64_t value = FIELD;                             \
    PACK_FIELD(64 - __builtin_clzg(value, 63), value);  \
    END_FIELD(1);                                       \
} while(0)

ssize_t nexusrv_msg_encode(const nexusrv_dencoder_cfg *cfg,
                           uint8_t *buffer, size_t limit,
                           const nexusrv_msg *msg) {
    size_t bit_offset = 0;
    unsigned cdf;
    PACK_FIXED(NEXUS_RV_BITS_TCODE, msg->common.tcode);
    if (msg->common.tcode != NEXUSRV_TCODE_IDLE && cfg->src_bits)
        PACK_FIXED(cfg->src_bits, msg->common.source);
    switch (msg->common.tcode) {
        case NENUSRV_TCODE_OWNERSHIP:
            PACK_FIXED(NEXUS_RV_BITS_OWNERSHIP_FMT, msg->ownership.format);
            PACK_FIXED(NEXUS_RV_BITS_OWNERSHIP_PRV, msg->ownership.prv);
            PACK_FIXED(NEXUS_RV_BITS_OWNERSHIP_V, msg->ownership.v);
            PACK_VAR(msg->ownership.context);
            break;
        case NEXUSRV_TCODE_DIRBR:
            PACK_VAR_REQ(msg->dirbr.icnt);
            break;
        case NEXUSRV_TCODE_INDIRBR:
            PACK_FIXED(NEXUS_RV_BITS_ADDR_BTYPE, msg->indirbr.btype);
            PACK_VAR_REQ(msg->indirbr.icnt);
            PACK_VAR_REQ(msg->indirbr.uaddr);
            break;
        case NEXUSRV_TCODE_ERROR:
            PACK_FIXED(NEXUS_RV_BITS_ETYPE, msg->error.etype);
            PACK_VAR(msg->error.ecode);
            break;
        case NEXUSRV_TCODE_PROG_SYNC:
            PACK_FIXED(NEXUS_RV_BITS_ADDR_SYNC, msg->prog_sync.sync);
            PACK_VAR_REQ(msg->prog_sync.icnt);
            PACK_VAR_REQ(msg->prog_sync.faddr);
            break;
        case NEXUSRV_TCODE_DIRBR_SYNC: // Same as NexusRvTcodeProgTraceSync
            PACK_FIXED(NEXUS_RV_BITS_ADDR_SYNC, msg->dirbr_sync.sync);
            PACK_VAR_REQ(msg->dirbr_sync.icnt);
            PACK_VAR_REQ(msg->dirbr_sync.faddr);
            break;
        case NEXUSRV_TCODE_INDIRBR_SYNC:
            PACK_FIXED(NEXUS_RV_BITS_ADDR_SYNC, msg->indirbr_sync.sync);
            PACK_FIXED(NEXUS_RV_BITS_ADDR_BTYPE, msg->indirbr_sync.btype);
            PACK_VAR_REQ(msg->indirbr_sync.icnt);
            PACK_VAR_REQ(msg->indirbr_sync.faddr);
            break;
        case NEXUSRV_TCODE_RESOURCE_FULL:
            PACK_FIXED(NEXUS_RV_BITS_RCODE, msg->resource_full.rcode);
            if (!msg->resource_full.rcode)
                PACK_VAR_REQ(msg->resource_full.icnt);
            else if (msg->resource_full.rcode > 2)
                PACK_VAR(msg->resource_full.rdata);
            else {
                PACK_VAR_REQ(msg->resource_full.hist);
                if (msg->resource_full.rcode == 2)
                    PACK_VAR_REQ(msg->resource_full.hrepeat);
            }
            break;
        case NEXUSRV_TCODE_INDIRBR_HIST:
            PACK_FIXED(NEXUS_RV_BITS_ADDR_BTYPE, msg->indirbr_hist.btype);
            PACK_VAR_REQ(msg->indirbr_hist.icnt);
            PACK_VAR_REQ(msg->indirbr_hist.uaddr);
            PACK_VAR_REQ(msg->indirbr_hist.hist);
            break;
        case NEXUSRV_TCODE_INDIRBR_HIST_SYNC:
            PACK_FIXED(NEXUS_RV_BITS_ADDR_SYNC, msg->indirbr_hist_sync.sync);
            PACK_FIXED(NEXUS_RV_BITS_ADDR_BTYPE, msg->indirbr_hist_sync.btype);
            PACK_VAR_REQ(msg->indirbr_hist_sync.icnt);
            PACK_VAR_REQ(msg->indirbr_hist_sync.faddr);
            PACK_VAR_REQ(msg->indirbr_hist_sync.hist);
            break;
        case NEXUSRV_TCODE_REPEAT_BR:
            PACK_VAR_REQ(msg->repeat_br.hrepeat);
            break;
        case NEXUSRV_TCODE_PROG_CORR:
            cdf = msg->prog_corr.icnt != 0;
            PACK_FIXED(NEXUS_RV_BITS_EVCODE, msg->prog_corr.evcode);
            PACK_FIXED(NEXUS_RV_BITS_CDF, cdf);
            PACK_VAR_REQ(msg->prog_corr.icnt);
            if (cdf == 1)
                PACK_VAR_REQ(msg->prog_corr.hist);
            break;
        case NEXUSRV_TCODE_IDLE:
            break;
        default:
            return -nexus_msg_unsupported;
    }
    if (cfg->has_timestamp)
        PACK_VAR_REQ(msg->common.timestamp);
    END_FIELD(3);
    return bit_offset / NEXUS_RV_MDO_BITS;
}