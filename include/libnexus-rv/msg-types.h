// SPDX-License-Identifer: Apache 2.0
/*
 * msg-types.h - Nexus-RV message definitions
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_MSG_TYPES_H
#define LIBNEXUS_RV_MSG_TYPES_H

#include <stdint.h>

#define NEXUS_RV_FIELD_TIME_SRC \
struct {                        \
    uint64_t TimeStamp : 54;    \
    uint64_t Source : 10;       \
}

#define NEXUS_RV_BITS_ADDR_SYNC 4
#define NEXUS_RV_FIELD_XADDR_SYNC   \
struct {                            \
    uint64_t XAddr : 58;            \
    uint64_t Sync: 4;               \
}

#define NEXUS_RV_BITS_ADDR_BTYPE 2
#define NEXUS_RV_FIELD_XADDR_BTYPE  \
struct {                            \
    uint64_t XAddr : 58;            \
    uint64_t : 4;                   \
    uint64_t BType : 2;             \
}

#define NEXUS_RV_FIELD_XADDR_SYNC_BTYPE \
struct {                                \
    uint64_t XAddr : 58;                \
    uint64_t Sync : 4;                  \
    uint64_t BType : 2;                 \
}

#define NEXUS_RV_BITS_TCODE 6
#define NEXUS_RV_FIELD_TCODE    \
struct {                        \
    uint8_t TCode: 6;           \
}

#define NEXUS_RV_BITS_ETYPE 4
#define NEXUS_RV_FIELD_TCODE_ETYPE  \
struct {                            \
    uint32_t TCode : 6;             \
    uint32_t EType : 4;             \
}

#define NEXUS_RV_FIELD_TCODE_ICNT   \
struct {                            \
    uint32_t TCode : 6;             \
    uint32_t : 4;                   \
    uint32_t ICnt : 22;             \
}

#define NEXUS_RV_FIELD_TCODE_REPEAT \
struct {                            \
    uint32_t TCode : 6;             \
    uint32_t : 4;                   \
    uint32_t HRepeat : 18;          \
}

#define NEXUS_RV_BITS_EVCODE 4
#define NEXUS_RV_BITS_CDF 2
#define NEXUS_RV_FIELD_TCODE_ICNT_EVCODE    \
struct {                                    \
    uint32_t TCode : 6;                     \
    uint32_t EVCode : 4;                    \
    uint32_t ICnt : 22;                     \
}

#define NEXUS_RV_BITS_RCODE 4
#define NEXUS_RV_FIELD_TCODE_ICNT_RCODE_REPEAT  \
union {                                         \
    struct {                                    \
        uint32_t TCode : 6;                     \
        uint32_t RCode : 4;                     \
    };                                          \
    struct {                                    \
        uint32_t : 10;                          \
        uint32_t ICnt : 22;                     \
    };                                          \
    struct {                                    \
        uint32_t : 10;                          \
        uint32_t HRepeat : 18;                  \
    };                                          \
}

#define NEXUS_RV_BITS_OWNERSHIP_FMT 2
#define NEXUS_RV_BITS_OWNERSHIP_PRV 2
#define NEXUS_RV_BITS_OWNERSHIP_V 1
#define NEXUS_RV_FIELD_OWNERSHIP    \
struct {                            \
    uint8_t Format : 2;             \
    uint8_t PRV : 2;                \
    uint8_t V : 1;                  \
}

typedef struct NexusRvMsgCommon {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE;
} NexusRvMsgCommon;

typedef struct NexusRvMsgOwnership{
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE;
    NEXUS_RV_FIELD_OWNERSHIP;
    uint64_t Context;
} NexusRvMsgOwnership;
#undef NEXUS_RV_FIELD_TCODE
#undef NEXUS_RV_FIELD_OWNERSHIP

typedef struct NexusRvMsgDirectBranch {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_ICNT;
} NexusRvMsgDirectBranch;

typedef struct NexusRvMsgIndirectBranch {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_ICNT;
    NEXUS_RV_FIELD_XADDR_BTYPE;
} NexusRvMsgIndirectBranch;

typedef struct NexusRvMsgProgTraceSync {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_ICNT;
    NEXUS_RV_FIELD_XADDR_SYNC;
} NexusRvMsgProgTraceSync;

typedef NexusRvMsgProgTraceSync NexusRvMsgDirectBranchSync;

typedef struct NexusRvMsgIndirectBranchSync {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_ICNT;
    NEXUS_RV_FIELD_XADDR_SYNC_BTYPE;
} NexusRvMsgIndirectBranchSync;

typedef struct NexusRvMsgIndirectBranchHist {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_ICNT;
    uint32_t Hist;
    NEXUS_RV_FIELD_XADDR_BTYPE;
} NexusRvMsgIndirectBranchHist;

typedef struct NexusRvMsgIndirectBranchHistSync {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_ICNT;
    uint32_t Hist;
    NEXUS_RV_FIELD_XADDR_SYNC_BTYPE;
} NexusRvMsgIndirectBranchHistSync;
#undef NEXUS_RV_FIELD_TCODE_ICNT
#undef NEXUS_RV_FIELD_XADDR_SYNC
#undef NEXUS_RV_FIELD_XADDR_BTYPE
#undef NEXUS_RV_FIELD_XADDR_SYNC_BTYPE

typedef struct NexusRvMsgResourceFull {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_ICNT_RCODE_REPEAT;
    union {
        uint32_t Hist;
        uint32_t RDATA;
    };
} NexusRvMsgResourceFull;
#undef NEXUS_RV_FIELD_TCODE_ICNT_RCODE_REPEAT

typedef struct NexusRvMsgRepeatBranch {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_REPEAT;
} NexusRvMsgRepeatBranch;
#undef NEXUS_RV_FIELD_TCODE_REPEAT

typedef struct NexusRvMsgProgTraceCorrelation {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_ICNT_EVCODE;
    uint32_t Hist;
} NexusRvMsgProgTraceCorrelation;
#undef NEXUS_RV_FIELD_TCODE_ICNT_EVCODE

typedef struct NexusRvMsgError {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_ETYPE;
    uint32_t Ecode;
} NexusRvMsgError;
#undef NEXUS_RV_FIELD_TCODE_ETYPE
#undef NEXUS_RV_FIELD_TIME_SRC

typedef union NexusRvMsg {
    NexusRvMsgCommon Common;
    NexusRvMsgOwnership Ownership;
    NexusRvMsgDirectBranch DirectBranch;
    NexusRvMsgIndirectBranch IndirectBranch;
    NexusRvMsgError Error;
    NexusRvMsgProgTraceSync ProgTraceSync;
    NexusRvMsgDirectBranchSync DirectBranchSync;
    NexusRvMsgIndirectBranchSync IndirectBranchSync;
    NexusRvMsgResourceFull ResourceFull;
    NexusRvMsgIndirectBranchHist IndirectBranchHist;
    NexusRvMsgIndirectBranchHistSync IndirectBranchHistSync;
    NexusRvMsgRepeatBranch RepeatBranch;
    NexusRvMsgProgTraceCorrelation ProgTraceCorrelation;
} NexusRvMsg;

enum NexusRvTcodes {
    NexusRvTcodeOwnership              = 2,
    NexusRvTcodeDirectBranch           = 3,
    NexusRvTcodeIndirectBranch         = 4,
    NexusRvTcodeError                  = 8,
    NexusRvTcodeProgTraceSync          = 9,
    NexusRvTcodeDirectBranchSync       = 11,
    NexusRvTcodeIndirectBranchSync     = 12,
    NexusRvTcodeResourceFull           = 27,
    NexusRvTcodeIndirectBranchHist     = 28,
    NexusRvTcodeIndirectBranchHistSync = 29,
    NexusRvTcodeRepeatBranch           = 30,
    NexusRvTcodeProgTraceCorrelation   = 33,
    NexusRvTcodeIdle                   = 63,
};

static inline const char* NexusRvTcodeStr(enum NexusRvTcodes Tcode) {
    switch (Tcode) {
        case NexusRvTcodeOwnership:
            return "Ownership";
        case NexusRvTcodeDirectBranch:
            return "DirectBranch";
        case NexusRvTcodeIndirectBranch:
            return "IndirectBranch";
        case NexusRvTcodeError:
            return "Error";
        case NexusRvTcodeProgTraceSync:
            return "ProgTraceSync";
        case NexusRvTcodeDirectBranchSync:
            return "DirectBranchSync";
        case NexusRvTcodeIndirectBranchSync:
            return "IndirectBranchSync";
        case NexusRvTcodeResourceFull:
            return "ResourceFull";
        case NexusRvTcodeIndirectBranchHist:
            return "IndirectBranchHist";
        case NexusRvTcodeIndirectBranchHistSync:
            return "IndirectBranchHistSync";
        case NexusRvTcodeRepeatBranch:
            return "RepeatBranch";
        case NexusRvTcodeProgTraceCorrelation:
            return "ProgTraceCorrelation";
        case NexusRvTcodeIdle:
            return "Idle";
        default:
            return "Unknown";
    }
}

#endif