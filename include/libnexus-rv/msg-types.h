// SPDX-License-Identifer: Apache 2.0
/*
 * msg-types.h - Nexus-RV message definitions
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_MSG_TYPES_H
#define LIBNEXUS_RV_MSG_TYPES_H

#include <stdint.h>
#include <stddef.h>

#define NEXUS_RV_FIELD_TIME_SRC \
struct {                        \
    uint64_t timestamp : 54;    \
    uint64_t source : 10;       \
}

#define NEXUS_RV_BITS_ADDR_SYNC 4
#define NEXUS_RV_FIELD_XADDR_SYNC   \
struct {                            \
    uint64_t faddr : 58;            \
    uint64_t sync: 4;               \
}

#define NEXUS_RV_BITS_ADDR_BTYPE 2
#define NEXUS_RV_FIELD_XADDR_BTYPE  \
struct {                            \
    uint64_t uaddr : 58;            \
    uint64_t : 4;                   \
    uint64_t btype : 2;             \
}

#define NEXUS_RV_FIELD_XADDR_SYNC_BTYPE \
struct {                                \
    uint64_t faddr : 58;                \
    uint64_t sync : 4;                  \
    uint64_t btype : 2;                 \
}

#define NEXUS_RV_BITS_TCODE 6
#define NEXUS_RV_FIELD_TCODE    \
struct {                        \
    uint8_t tcode: 6;           \
}

#define NEXUS_RV_BITS_ETYPE 4
#define NEXUS_RV_FIELD_TCODE_ETYPE  \
struct {                            \
    uint32_t tcode : 6;             \
    uint32_t etype : 4;             \
}

#define NEXUS_RV_FIELD_TCODE_ICNT   \
struct {                            \
    uint32_t tcode : 6;             \
    uint32_t : 4;                   \
    uint32_t icnt : 22;             \
}

#define NEXUS_RV_FIELD_TCODE_REPEAT \
struct {                            \
    uint32_t tcode : 6;             \
    uint32_t : 4;                   \
    uint32_t hrepeat : 18;          \
}

#define NEXUS_RV_BITS_EVCODE 4
#define NEXUS_RV_BITS_CDF 2
#define NEXUS_RV_FIELD_TCODE_ICNT_EVCODE    \
struct {                                    \
    uint32_t tcode : 6;                     \
    uint32_t evcode : 4;                    \
    uint32_t icnt : 22;                     \
}

#define NEXUS_RV_BITS_RCODE 4
#define NEXUS_RV_FIELD_TCODE_ICNT_RCODE_REPEAT  \
union {                                         \
    struct {                                    \
        uint32_t tcode : 6;                     \
        uint32_t rcode : 4;                     \
    };                                          \
    struct {                                    \
        uint32_t : 10;                          \
        uint32_t icnt : 22;                     \
    };                                          \
    struct {                                    \
        uint32_t : 10;                          \
        uint32_t hrepeat : 18;                  \
    };                                          \
}

#define NEXUS_RV_BITS_OWNERSHIP_FMT 2
#define NEXUS_RV_BITS_OWNERSHIP_PRV 2
#define NEXUS_RV_BITS_OWNERSHIP_V 1
#define NEXUS_RV_FIELD_OWNERSHIP    \
struct {                            \
    uint8_t format : 2;             \
    uint8_t prv : 2;                \
    uint8_t v : 1;                  \
}

typedef struct nexusrv_msg_common {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE;
} nexusrv_msg_common;

typedef struct nexusrv_msg_ownership{
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE;
    NEXUS_RV_FIELD_OWNERSHIP;
    uint64_t context;
} nexusrv_msg_ownership;
#undef NEXUS_RV_FIELD_TCODE
#undef NEXUS_RV_FIELD_OWNERSHIP

typedef struct nexusrv_msg_dirbr {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_ICNT;
} nexusrv_msg_dirbr;

typedef struct nexusrv_msg_indirbr {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_ICNT;
    NEXUS_RV_FIELD_XADDR_BTYPE;
} nexusrv_msg_indirbr;

typedef struct nexusrv_msg_prog_sync {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_ICNT;
    NEXUS_RV_FIELD_XADDR_SYNC;
} nexusrv_msg_prog_sync;

typedef nexusrv_msg_prog_sync nexusrv_msg_dirbr_sync;

typedef struct nexusrv_msg_indirbr_sync {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_ICNT;
    NEXUS_RV_FIELD_XADDR_SYNC_BTYPE;
} nexusrv_msg_indirbr_sync;

typedef struct nexusrv_msg_indirbr_hist {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_ICNT;
    uint32_t hist;
    NEXUS_RV_FIELD_XADDR_BTYPE;
} nexusrv_msg_indirbr_hist;

typedef struct nexusrv_msg_indirbr_hist_sync {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_ICNT;
    uint32_t hist;
    NEXUS_RV_FIELD_XADDR_SYNC_BTYPE;
} nexusrv_msg_indirbr_hist_sync;
#undef NEXUS_RV_FIELD_TCODE_ICNT
#undef NEXUS_RV_FIELD_XADDR_SYNC
#undef NEXUS_RV_FIELD_XADDR_BTYPE
#undef NEXUS_RV_FIELD_XADDR_SYNC_BTYPE

typedef struct nexusrv_msg_resource_full {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_ICNT_RCODE_REPEAT;
    union {
        uint32_t hist;
        uint32_t rdata;
    };
} nexusrv_msg_resource_full;
#undef NEXUS_RV_FIELD_TCODE_ICNT_RCODE_REPEAT

typedef struct nexusrv_msg_repeat_br {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_REPEAT;
} nexusrv_msg_repeat_br;
#undef NEXUS_RV_FIELD_TCODE_REPEAT

typedef struct nexusrv_msg_prog_corr {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_ICNT_EVCODE;
    uint32_t hist;
} nexusrv_msg_prog_corr;
#undef NEXUS_RV_FIELD_TCODE_ICNT_EVCODE

typedef struct nexusrv_msg_error {
    NEXUS_RV_FIELD_TIME_SRC;
    NEXUS_RV_FIELD_TCODE_ETYPE;
    uint32_t ecode;
} nexusrv_msg_error;
#undef NEXUS_RV_FIELD_TCODE_ETYPE
#undef NEXUS_RV_FIELD_TIME_SRC

/* Nexus-RV Msg types */
typedef union nexusrv_msg {
    nexusrv_msg_common common;
    nexusrv_msg_ownership ownership;
    nexusrv_msg_dirbr dirbr;
    nexusrv_msg_indirbr indirbr;
    nexusrv_msg_dirbr_sync dirbr_sync;
    nexusrv_msg_indirbr_sync indirbr_sync;
    nexusrv_msg_indirbr_hist indirbr_hist;
    nexusrv_msg_indirbr_hist_sync indirbr_hist_sync;
    nexusrv_msg_resource_full resource_full;
    nexusrv_msg_prog_sync prog_sync;
    nexusrv_msg_prog_corr prog_corr;
    nexusrv_msg_repeat_br repeat_br;
    nexusrv_msg_error error;
} nexusrv_msg;

enum nexus_rv_tcodes {
    NENUSRV_TCODE_OWNERSHIP             = 2,
    NEXUSRV_TCODE_DIRBR                 = 3,
    NEXUSRV_TCODE_INDIRBR               = 4,
    NEXUSRV_TCODE_ERROR                 = 8,
    NEXUSRV_TCODE_PROG_SYNC             = 9,
    NEXUSRV_TCODE_DIRBR_SYNC            = 11,
    NEXUSRV_TCODE_INDIRBR_SYNC          = 12,
    NEXUSRV_TCODE_RESOURCE_FULL         = 27,
    NEXUSRV_TCODE_INDIRBR_HIST          = 28,
    NEXUSRV_TCODE_INDIRBR_HIST_SYNC     = 29,
    NEXUSRV_TCODE_REPEAT_BR             = 30,
    NEXUSRV_TCODE_PROG_CORR             = 33,
    NEXUSRV_TCODE_IDLE                  = 63,
};

static inline const char* nexusrv_tcode_str(enum nexus_rv_tcodes tcode) {
    switch (tcode) {
        case NENUSRV_TCODE_OWNERSHIP:
            return "Ownership";
        case NEXUSRV_TCODE_DIRBR:
            return "DirectBranch";
        case NEXUSRV_TCODE_INDIRBR:
            return "IndirectBranch";
        case NEXUSRV_TCODE_ERROR:
            return "Error";
        case NEXUSRV_TCODE_PROG_SYNC:
            return "ProgTraceSync";
        case NEXUSRV_TCODE_DIRBR_SYNC:
            return "DirectBranchSync";
        case NEXUSRV_TCODE_INDIRBR_SYNC:
            return "IndirectBranchSync";
        case NEXUSRV_TCODE_RESOURCE_FULL:
            return "ResourceFull";
        case NEXUSRV_TCODE_INDIRBR_HIST:
            return "IndirectBranchHist";
        case NEXUSRV_TCODE_INDIRBR_HIST_SYNC:
            return "IndirectBranchHistSync";
        case NEXUSRV_TCODE_REPEAT_BR:
            return "RepeatBranch";
        case NEXUSRV_TCODE_PROG_CORR:
            return "ProgTraceCorrelation";
        case NEXUSRV_TCODE_IDLE:
            return "Idle";
        default:
            return "Unknown";
    }
}

static inline size_t nexusrv_msg_sz(const nexusrv_msg *msg) {
    switch (msg->common.tcode) {
        case NENUSRV_TCODE_OWNERSHIP:
            return sizeof(nexusrv_msg_ownership);
        case NEXUSRV_TCODE_DIRBR:
            return sizeof(nexusrv_msg_dirbr);
        case NEXUSRV_TCODE_INDIRBR:
            return sizeof(nexusrv_msg_indirbr);
        case NEXUSRV_TCODE_ERROR:
            return sizeof(nexusrv_msg_error);
        case NEXUSRV_TCODE_PROG_SYNC:
            return sizeof(nexusrv_msg_prog_sync);
        case NEXUSRV_TCODE_DIRBR_SYNC:
            return sizeof(nexusrv_msg_dirbr_sync);
        case NEXUSRV_TCODE_INDIRBR_SYNC:
            return sizeof(nexusrv_msg_indirbr_sync);
        case NEXUSRV_TCODE_RESOURCE_FULL:
            return sizeof(nexusrv_msg_resource_full);
        case NEXUSRV_TCODE_INDIRBR_HIST:
            return sizeof(nexusrv_msg_indirbr_hist);
        case NEXUSRV_TCODE_INDIRBR_HIST_SYNC:
            return sizeof(nexusrv_msg_indirbr_hist_sync);
        case NEXUSRV_TCODE_REPEAT_BR:
            return sizeof(nexusrv_msg_repeat_br);
        case NEXUSRV_TCODE_PROG_CORR:
            return sizeof(nexusrv_msg_prog_corr);
        default:
            return sizeof(nexusrv_msg_common);
    }
}

#endif