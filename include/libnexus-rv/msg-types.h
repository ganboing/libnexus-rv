// SPDX-License-Identifer: Apache 2.0
/*
 * msg-types.h - NexusRV message definitions
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#ifndef LIBNEXUS_RV_MSG_TYPES_H
#define LIBNEXUS_RV_MSG_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/** @file */

/** Number of TCODE bits */
#define NEXUS_RV_BITS_TCODE 6
/** Number of ETYPE bits */
#define NEXUS_RV_BITS_ETYPE 4
/** Number of RCODE bits */
#define NEXUS_RV_BITS_RCODE 4
/** Number of EVCODE bits */
#define NEXUS_RV_BITS_EVCODE 4
/** Number of CDF bits */
#define NEXUS_RV_BITS_CDF 2
/** Number of SYNC bits */
#define NEXUS_RV_BITS_ADDR_SYNC 4
/** Number of B-TYPE bits */
#define NEXUS_RV_BITS_ADDR_BTYPE 2
/** Number of PROCESS.FORMAT bits */
#define NEXUS_RV_BITS_OWNERSHIP_FMT 2
/** Number of PROCESS.PRV bits */
#define NEXUS_RV_BITS_OWNERSHIP_PRV 2
/** Number of PROCESS.V bits */
#define NEXUS_RV_BITS_OWNERSHIP_V 1

/** @brief TCODE enumeration
 */
enum nexusrv_tcodes {
    NEXUSRV_TCODE_Ownership                 = 2,
    NEXUSRV_TCODE_DirectBranch              = 3,
    NEXUSRV_TCODE_IndirectBranch            = 4,
    NEXUSRV_TCODE_Error                     = 8,
    NEXUSRV_TCODE_ProgTraceSync             = 9,
    NEXUSRV_TCODE_DirectBranchSync          = 11,
    NEXUSRV_TCODE_IndirectBranchSync        = 12,
    NEXUSRV_TCODE_ResourceFull              = 27,
    NEXUSRV_TCODE_IndirectBranchHist        = 28,
    NEXUSRV_TCODE_IndirectBranchHistSync    = 29,
    NEXUSRV_TCODE_RepeatBranch              = 30,
    NEXUSRV_TCODE_ProgTraceCorrelation      = 33,
    NEXUSRV_TCODE_VendorStart               = 56,
    NEXUSRV_TCODE_VendorLast                = 62,
    NEXUSRV_TCODE_Idle                      = 63,
};

static inline const char* nexusrv_tcode_str(enum nexusrv_tcodes tcode) {
    switch (tcode) {
        case NEXUSRV_TCODE_Ownership:
            return "Ownership";
        case NEXUSRV_TCODE_DirectBranch:
            return "DirectBranch";
        case NEXUSRV_TCODE_IndirectBranch:
            return "IndirectBranch";
        case NEXUSRV_TCODE_Error:
            return "Error";
        case NEXUSRV_TCODE_ProgTraceSync:
            return "ProgTraceSync";
        case NEXUSRV_TCODE_DirectBranchSync:
            return "DirectBranchSync";
        case NEXUSRV_TCODE_IndirectBranchSync:
            return "IndirectBranchSync";
        case NEXUSRV_TCODE_ResourceFull:
            return "ResourceFull";
        case NEXUSRV_TCODE_IndirectBranchHist:
            return "IndirectBranchHist";
        case NEXUSRV_TCODE_IndirectBranchHistSync:
            return "IndirectBranchHistSync";
        case NEXUSRV_TCODE_RepeatBranch:
            return "RepeatBranch";
        case NEXUSRV_TCODE_ProgTraceCorrelation:
            return "ProgTraceCorrelation";
        case NEXUSRV_TCODE_Idle:
            return "Idle";
        default:
            return "Unknown";
    }
}

/** @brief Decoded NexusRV Message
 */
typedef struct nexusrv_msg {
    uint64_t timestamp; /*!< Absolute or Delta timestamp of the Message */
    uint16_t src;       /*!< SRC field */
    uint8_t tcode;      /*!< TCODE field */
    union {
        struct {
            uint8_t sync_type : 4;   /*!< SYNC field */
            uint8_t branch_type : 2; /*!< B-TYPE field */
        };
        struct {
            uint8_t error_type : 4;  /*!< ETYPE field */
        };
        struct {
            uint8_t res_code : 4;    /*!< RCODE field */
        };
        struct {
            uint8_t stop_code : 4;   /*!< EVCODE field */
            uint8_t cdf : 2;         /*!< CDF field */
        };
        struct {
            uint8_t ownership_fmt : 2;  /*!< PROCESS.FORMAT field */
            uint8_t ownership_priv : 2; /*!< PROCESS.PRV field */
            uint8_t ownership_v : 1;    /*!< PROCESS.V field */
        };
    };
    union {
        uint32_t icnt;        /*!< I-CNT field */
        uint32_t error_code;  /*!< ECODE field */
        uint32_t res_data;    /*!< RDATA field */
    };
    uint32_t hist;     /*!< HIST field */
    uint32_t hrepeat;  /*!< HREPEAT field, or synthesized  HREPEAT */
    union {
        uint64_t xaddr;   /*!< x-ADDR */
        uint64_t context; /*!< PROCESS.CONTEXT */
    };
} nexusrv_msg;

static inline bool nexusrv_msg_known(const nexusrv_msg *msg) {
    switch (msg->tcode) {
        case NEXUSRV_TCODE_Idle:
        case NEXUSRV_TCODE_ResourceFull:
        case NEXUSRV_TCODE_DirectBranch:
        case NEXUSRV_TCODE_DirectBranchSync:
        case NEXUSRV_TCODE_IndirectBranch:
        case NEXUSRV_TCODE_IndirectBranchSync:
        case NEXUSRV_TCODE_IndirectBranchHist:
        case NEXUSRV_TCODE_IndirectBranchHistSync:
        case NEXUSRV_TCODE_RepeatBranch:
        case NEXUSRV_TCODE_Error:
        case NEXUSRV_TCODE_Ownership:
        case NEXUSRV_TCODE_ProgTraceSync:
            return true;
        case NEXUSRV_TCODE_ProgTraceCorrelation:
            return msg->cdf < 2;
        default:
            return false;
    }
}

static inline bool nexusrv_msg_idle(const nexusrv_msg *msg) {
    return msg->tcode == NEXUSRV_TCODE_Idle;
}

static inline bool nexusrv_msg_has_src(const nexusrv_msg *msg) {
    return !nexusrv_msg_idle(msg);
}

static inline bool nexusrv_msg_is_branch(const nexusrv_msg *msg) {
    switch (msg->tcode) {
        case NEXUSRV_TCODE_DirectBranch:
        case NEXUSRV_TCODE_DirectBranchSync:
        case NEXUSRV_TCODE_IndirectBranch:
        case NEXUSRV_TCODE_IndirectBranchSync:
        case NEXUSRV_TCODE_IndirectBranchHist:
        case NEXUSRV_TCODE_IndirectBranchHistSync:
            return true;
        default:
            return false;
    }
}

static inline bool nexusrv_msg_is_indir_branch(const nexusrv_msg *msg) {
    switch (msg->tcode) {
        case NEXUSRV_TCODE_IndirectBranch:
        case NEXUSRV_TCODE_IndirectBranchSync:
        case NEXUSRV_TCODE_IndirectBranchHist:
        case NEXUSRV_TCODE_IndirectBranchHistSync:
            return true;
        default:
            return false;
    }
}

static inline bool nexusrv_msg_is_res(const nexusrv_msg *msg) {
    return msg->tcode == NEXUSRV_TCODE_ResourceFull;
}

static inline bool nexusrv_msg_is_sync(const nexusrv_msg *msg) {
    switch (msg->tcode) {
        case NEXUSRV_TCODE_DirectBranchSync:
        case NEXUSRV_TCODE_IndirectBranchSync:
        case NEXUSRV_TCODE_IndirectBranchHistSync:
        case NEXUSRV_TCODE_ProgTraceSync:
            return true;
        default:
            return false;
    }
}

static inline bool nexusrv_msg_is_error(const nexusrv_msg *msg) {
    return msg->tcode == NEXUSRV_TCODE_Error;
}

static inline bool nexusrv_msg_is_stop(const nexusrv_msg *msg) {
    return msg->tcode == NEXUSRV_TCODE_ProgTraceCorrelation;
}

static inline bool nexusrv_msg_has_icnt(const nexusrv_msg *msg) {
    switch (msg->tcode) {
        case NEXUSRV_TCODE_ResourceFull:
            return !msg->res_code;
        case NEXUSRV_TCODE_DirectBranch:
        case NEXUSRV_TCODE_DirectBranchSync:
        case NEXUSRV_TCODE_IndirectBranch:
        case NEXUSRV_TCODE_IndirectBranchSync:
        case NEXUSRV_TCODE_IndirectBranchHist:
        case NEXUSRV_TCODE_IndirectBranchHistSync:
        case NEXUSRV_TCODE_ProgTraceSync:
        case NEXUSRV_TCODE_ProgTraceCorrelation:
            return true;
        default:
            return false;
    }
}

static inline bool nexusrv_msg_has_xaddr(const nexusrv_msg *msg) {
    switch (msg->tcode) {
        case NEXUSRV_TCODE_IndirectBranch:
        case NEXUSRV_TCODE_IndirectBranchSync:
        case NEXUSRV_TCODE_IndirectBranchHist:
        case NEXUSRV_TCODE_IndirectBranchHistSync:
        case NEXUSRV_TCODE_DirectBranchSync:
        case NEXUSRV_TCODE_ProgTraceSync:
            return true;
        default:
            return false;
    }
}

static inline bool nexusrv_msg_has_hist(const nexusrv_msg *msg) {
    switch (msg->tcode) {
        case NEXUSRV_TCODE_ResourceFull:
            return msg->res_code == 1 || msg->res_code == 2;
        case NEXUSRV_TCODE_ProgTraceCorrelation:
            return msg->cdf == 1;
        case NEXUSRV_TCODE_IndirectBranchHist:
        case NEXUSRV_TCODE_IndirectBranchHistSync:
            return true;
        default:
            return false;
    }
}

static inline bool nexusrv_msg_known_rescode(uint8_t rescode) {
    return rescode < 3;
}

static inline unsigned nexusrv_msg_hist_bits(uint32_t hist) {
    if (!hist)
        return 0;
    return sizeof(long) * 8 - 1 - __builtin_clzl(hist);
}

#endif