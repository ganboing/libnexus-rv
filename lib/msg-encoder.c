// SPDX-License-Identifer: Apache 2.0
/*
 * msg-encoder.c - NexusRV message encoder implementation
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <assert.h>
#include <libnexus-rv/error.h>
#include <libnexus-rv/msg-encoder.h>
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
        if (available >= bits)
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
    /* Count the number of 1-bit in value */            \
    PACK_FIELD(64 - __builtin_clzg(value, 64), value);  \
    END_FIELD(1);                                       \
} while(0)

#define PACK_VAR_REQ(FIELD)                             \
do {                                                    \
    uint64_t value = FIELD;                             \
    /* Count the number of 1-bit in value, min=1 */     \
    PACK_FIELD(64 - __builtin_clzg(value, 63), value);  \
    END_FIELD(1);                                       \
} while(0)

#define PACK_XADDR_VAO(FIELD)                           \
do {                                                    \
    uint64_t xaddr = FIELD;                             \
    size_t bits = 64 - __builtin_clzg(xaddr, 64);       \
    if (bits < 64)  /* MSB is 0 */                      \
        /* bits to encode x-addr with sign-ext */       \
        ++bits;                                         \
    else /* MSB is 1 */                                 \
        /* bits to encode x-addr with sign-ext */       \
        bits = 65 - __builtin_clzg(~xaddr, 64);         \
    /* Align to byte boundary */                        \
    size_t bytes = (bit_offset + bits +                 \
        NEXUS_RV_MDO_BITS - 1) / NEXUS_RV_MDO_BITS;     \
    bits = bytes * NEXUS_RV_MDO_BITS - bit_offset;      \
    if (bits > 64)                                      \
        bits = 64;                                      \
    PACK_FIELD(bits, xaddr);                            \
    END_FIELD(1);                                       \
} while (0)

ssize_t nexusrv_msg_encode(const nexusrv_hw_cfg *hwcfg,
                           uint8_t *buffer, size_t limit,
                           const nexusrv_msg *msg) {
    size_t bit_offset = 0;
    if (!nexusrv_msg_known(msg))
        return -nexus_msg_unsupported;
    PACK_FIXED(NEXUS_RV_BITS_TCODE, msg->tcode);
    if (msg->tcode == NEXUSRV_TCODE_Idle)
        goto done;
    if (hwcfg->src_bits)
        PACK_FIXED(hwcfg->src_bits, msg->src);
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
        PACK_FIXED(NEXUS_RV_BITS_ADDR_SYNC, msg->sync_type);
    if (nexusrv_msg_is_indir_branch(msg))
        PACK_FIXED(NEXUS_RV_BITS_ADDR_BTYPE, msg->branch_type);
    assert(nexusrv_msg_has_icnt(msg));
    PACK_VAR_REQ(msg->icnt);
    if (nexusrv_msg_has_xaddr(msg))
        if (hwcfg->VAO)
            PACK_XADDR_VAO(msg->xaddr);
        else
            PACK_VAR_REQ(msg->xaddr);
    if (nexusrv_msg_has_hist(msg))
        PACK_VAR_REQ(msg->hist);
    goto finished_common;
handle_rest:
    switch (msg->tcode) {
        default:
            return -nexus_msg_unsupported;
        case NEXUSRV_TCODE_Ownership:
            PACK_FIXED(NEXUS_RV_BITS_OWNERSHIP_FMT, msg->ownership_fmt);
            PACK_FIXED(NEXUS_RV_BITS_OWNERSHIP_PRV, msg->ownership_priv);
            PACK_FIXED(NEXUS_RV_BITS_OWNERSHIP_V, msg->ownership_v);
            PACK_VAR(msg->context);
            break;
        case NEXUSRV_TCODE_Error:
            PACK_FIXED(NEXUS_RV_BITS_ETYPE, msg->error_type);
            PACK_VAR(msg->error_code);
            break;
        case NEXUSRV_TCODE_ResourceFull:
            PACK_FIXED(NEXUS_RV_BITS_RCODE, msg->res_code);
            if (msg->res_code > 2) {
                PACK_VAR(msg->res_data);
                break;
            }
            switch (msg->res_code) {
                case 0:
                    PACK_VAR_REQ(msg->icnt);
                    break;
                case 1:
                    PACK_VAR_REQ(msg->hist);
                    break;
                case 2:
                    PACK_VAR_REQ(msg->hist);
                    PACK_VAR_REQ(msg->hrepeat);
                    break;
            }
            break;
        case NEXUSRV_TCODE_RepeatBranch:
            PACK_VAR_REQ(msg->hrepeat);
            break;
        case NEXUSRV_TCODE_ProgTraceCorrelation:
            PACK_FIXED(NEXUS_RV_BITS_EVCODE, msg->stop_code);
            PACK_FIXED(NEXUS_RV_BITS_CDF, msg->cdf);
            PACK_VAR_REQ(msg->icnt);
            if (msg->cdf > 1)
                return -nexus_msg_unsupported;
            if (msg->cdf)
                PACK_VAR_REQ(msg->hist);
            break;
    }
finished_common:
    if (hwcfg->ts_bits) {
        if (nexusrv_msg_is_sync(msg))
            PACK_VAR_REQ(msg->timestamp);
        else
            PACK_VAR(msg->timestamp);
    }
done:
    END_FIELD(3);
    return bit_offset / NEXUS_RV_MDO_BITS;
}