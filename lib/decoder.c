// SPDX-License-Identifer: Apache 2.0
/*
 * decoder.c - Nexus-RV decoder implementation
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <error.h>
#include <libnexus-rv/decoder.h>
#include <libnexus-rv/internal/protocol.h>

static ssize_t consume_bytes(const uint8_t *Buffer, size_t Limit, bool *Eom) {
    for (size_t Consumed = 0; Consumed < Limit;) {
        NexusRvMsgByte MsgByte = {Buffer[Consumed++]};
        if (MsgByte.MSEO == 2) // MESO == 2, reserved
            return -nexus_bad_mseo;
        if (!MsgByte.MSEO) // Msg continues
            continue;
        *Eom = MsgByte.MSEO == 3; // End of Msg
        return Consumed;
    }
    return -nexus_stream_truncate;
}

static uint64_t extract_bits(const uint8_t *Buffer, unsigned BitsOffset, unsigned Bits) {
    uint64_t Value = 0;
    unsigned StartByte = BitsOffset / NEXUS_RV_MDO_BITS;
    unsigned LastByte = (BitsOffset + Bits - 1) / NEXUS_RV_MDO_BITS;
    for (;; --LastByte) {
        NexusRvMsgByte MsgByte = {Buffer[LastByte]};
        if (StartByte != LastByte) {
            Value <<= NEXUS_RV_MDO_BITS;
            Value |= MsgByte.MDO;
            continue;
        }
        unsigned Shift = BitsOffset % NEXUS_RV_MDO_BITS;
        Value <<= (NEXUS_RV_MDO_BITS - Shift);
        Value |= (MsgByte.MDO >> Shift);
        break;
    }
    Value &= -1ULL >> (64 - Bits);
    return Value;
}

#define UNPACK_FIXED(FIELD_BITS, FIELD)                 \
do {                                                    \
    unsigned BitsLeft =                                 \
        ConsumedBytes * NEXUS_RV_MDO_BITS - BitOffset;  \
    if (BitsLeft < FIELD_BITS)                          \
        return -nexus_msg_missing_field;                \
    FIELD = extract_bits(Buffer - ConsumedBytes,        \
                        BitOffset, FIELD_BITS);         \
    BitOffset += FIELD_BITS;                            \
} while(0)

#define UNPACK_VAR(FIELD)                               \
({                                                      \
    unsigned BitsLeft =                                 \
        ConsumedBytes * NEXUS_RV_MDO_BITS - BitOffset;  \
    if (!BitsLeft)                                      \
        FIELD = 0;                                      \
    else                                                \
        FIELD = extract_bits(Buffer - ConsumedBytes,    \
                            BitOffset, BitsLeft);       \
    BitOffset += BitsLeft;                              \
    BitsLeft;                                           \
})

#define UNPACK_VAR_REQ(FIELD)               \
do {                                        \
    if (!UNPACK_VAR(FIELD))                 \
        return -nexus_msg_missing_field;    \
} while(0)

#define CONSUME_BYTES()                                 \
do {                                                    \
    if (Eom)                                            \
        return -nexus_msg_missing_field;                \
    ConsumedBytes = consume_bytes(Buffer, Limit, &Eom); \
    if (ConsumedBytes < 0)                              \
        return ConsumedBytes;                           \
    Buffer += ConsumedBytes;                            \
    Limit -= ConsumedBytes;                             \
    BitOffset = 0;                                      \
}                                                       \
while(0)

ssize_t nexusrv_decode(const NexusRvDecoderCfg *Cfg,
                 const uint8_t *Buffer, size_t Limit, NexusRvMsg *Msg) {
    bool Eom = false;
    const uint8_t *OrigBuffer = Buffer;
    ssize_t ConsumedBytes;
    unsigned BitOffset;
    unsigned CDF;
    CONSUME_BYTES();
    UNPACK_FIXED(NEXUS_RV_BITS_TCODE, Msg->Common.TCode);
    Msg->Common.Source = 0;
    Msg->Common.TimeStamp = 0;
    if (Cfg->SrcBits)
        UNPACK_FIXED(Cfg->SrcBits, Msg->Common.Source);
    switch (Msg->Common.TCode) {
        case NexusRvTcodeOwnership:
            UNPACK_FIXED(NEXUS_RV_BITS_OWNERSHIP_FMT, Msg->Ownership.Format);
            UNPACK_FIXED(NEXUS_RV_BITS_OWNERSHIP_PRV, Msg->Ownership.PRV);
            UNPACK_FIXED(NEXUS_RV_BITS_OWNERSHIP_V, Msg->Ownership.V);
            UNPACK_VAR(Msg->Ownership.Context);
            break;
        case NexusRvTcodeDirectBranch:
            UNPACK_VAR_REQ(Msg->DirectBranch.ICnt);
            break;
        case NexusRvTcodeIndirectBranch:
            UNPACK_FIXED(NEXUS_RV_BITS_ADDR_BTYPE, Msg->IndirectBranch.BType);
            UNPACK_VAR_REQ(Msg->IndirectBranch.ICnt);
            CONSUME_BYTES();
            UNPACK_VAR_REQ(Msg->IndirectBranch.XAddr);
            break;
        case NexusRvTcodeError:
            UNPACK_FIXED(NEXUS_RV_BITS_ETYPE, Msg->Error.EType);
            UNPACK_VAR(Msg->Error.Ecode);
            break;
        case NexusRvTcodeProgTraceSync:
            UNPACK_FIXED(NEXUS_RV_BITS_ADDR_SYNC, Msg->ProgTraceSync.Sync);
            UNPACK_VAR_REQ(Msg->ProgTraceSync.ICnt);
            CONSUME_BYTES();
            UNPACK_VAR_REQ(Msg->ProgTraceSync.XAddr);
            break;
        case NexusRvTcodeDirectBranchSync: // Same as NexusRvTcodeProgTraceSync
            UNPACK_FIXED(NEXUS_RV_BITS_ADDR_SYNC, Msg->DirectBranchSync.Sync);
            UNPACK_VAR_REQ(Msg->DirectBranchSync.ICnt);
            CONSUME_BYTES();
            UNPACK_VAR_REQ(Msg->DirectBranchSync.XAddr);
            break;
        case NexusRvTcodeIndirectBranchSync:
            UNPACK_FIXED(NEXUS_RV_BITS_ADDR_SYNC, Msg->IndirectBranchSync.Sync);
            UNPACK_FIXED(NEXUS_RV_BITS_ADDR_BTYPE, Msg->IndirectBranchSync.BType);
            UNPACK_VAR_REQ(Msg->IndirectBranchSync.ICnt);
            CONSUME_BYTES();
            UNPACK_VAR_REQ(Msg->IndirectBranchSync.XAddr);
            break;
        case NexusRvTcodeResourceFull:
            UNPACK_FIXED(NEXUS_RV_BITS_RCODE, Msg->ResourceFull.RCode);
            if (!Msg->ResourceFull.RCode)
                UNPACK_VAR_REQ(Msg->ResourceFull.ICnt);
            else if (Msg->ResourceFull.RCode > 2)
                UNPACK_VAR(Msg->ResourceFull.RDATA);
            else {
                UNPACK_VAR_REQ(Msg->ResourceFull.Hist);
                if (Msg->ResourceFull.RCode == 2) {
                    CONSUME_BYTES();
                    UNPACK_VAR_REQ(Msg->ResourceFull.HRepeat);
                }
            }
            break;
        case NexusRvTcodeIndirectBranchHist:
            UNPACK_FIXED(NEXUS_RV_BITS_ADDR_BTYPE, Msg->IndirectBranchHist.BType);
            UNPACK_VAR_REQ(Msg->IndirectBranchHist.ICnt);
            CONSUME_BYTES();
            UNPACK_VAR_REQ(Msg->IndirectBranchHist.XAddr);
            CONSUME_BYTES();
            UNPACK_VAR_REQ(Msg->IndirectBranchHist.Hist);
            break;
        case NexusRvTcodeIndirectBranchHistSync:
            UNPACK_FIXED(NEXUS_RV_BITS_ADDR_SYNC, Msg->IndirectBranchHistSync.Sync);
            UNPACK_FIXED(NEXUS_RV_BITS_ADDR_BTYPE, Msg->IndirectBranchHistSync.BType);
            UNPACK_VAR_REQ(Msg->IndirectBranchHistSync.ICnt);
            CONSUME_BYTES();
            UNPACK_VAR_REQ(Msg->IndirectBranchHistSync.XAddr);
            CONSUME_BYTES();
            UNPACK_VAR_REQ(Msg->IndirectBranchHistSync.Hist);
            break;
        case NexusRvTcodeRepeatBranch:
            UNPACK_VAR_REQ(Msg->RepeatBranch.HRepeat);
            break;
        case NexusRvTcodeProgTraceCorrelation:
            UNPACK_FIXED(NEXUS_RV_BITS_EVCODE, Msg->ProgTraceCorrelation.EVCode);
            UNPACK_FIXED(NEXUS_RV_BITS_CDF, CDF);
            UNPACK_VAR_REQ(Msg->ProgTraceCorrelation.ICnt);
            Msg->ProgTraceCorrelation.Hist = 0;
            if (CDF == 1) {
                CONSUME_BYTES();
                UNPACK_VAR_REQ(Msg->ProgTraceCorrelation.Hist);
            }
            break;
        case NexusRvTcodeIdle: // No timestamp
            if (!Eom)
                return -nexus_msg_invalid;
            return Buffer - OrigBuffer;
        default:
            error(0, 0, "Unknown Msg type %hhu\n", Msg->Common.TCode);
            break;
    }
    if (Cfg->HasTimeStamp && Eom)
        return -nexus_msg_missing_field;
    while (!Eom)
        CONSUME_BYTES();
    if (Cfg->HasTimeStamp)
        UNPACK_VAR_REQ(Msg->Common.TimeStamp);
    return Buffer - OrigBuffer;
}