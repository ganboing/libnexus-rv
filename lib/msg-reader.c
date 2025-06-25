// SPDX-License-Identifier: Apache 2.0
/*
 * msg-reader.c - NexusRV message reader
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <stdio.h>
#include <assert.h>
#include <libnexus-rv/error.h>
#include <libnexus-rv/msg-decoder.h>
#include "msg-format.h"

#define CHECK_SCANF(FMT, TYPE, FIELD)       \
do {                                        \
    TYPE value = 0;                         \
    int rc = fscanf(fp, FMT, &value);       \
    if (rc <= 0)                            \
        return -nexus_msg_missing_field;    \
    FIELD = value;                          \
} while(0)

int nexusrv_read_msg(FILE *fp, nexusrv_msg *msg) {
    CHECK_SCANF(NEXUS_FMT_TIMESTAMP, uint64_t, msg->timestamp);
    CHECK_SCANF(NEXUS_FMT_TCODE, unsigned, msg->tcode);
    if (msg->tcode == NEXUSRV_TCODE_Idle)
        return 0;
    CHECK_SCANF(NEXUS_FMT_SRC, unsigned, msg->src);
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
        CHECK_SCANF(NEXUS_FMT_SYNC, unsigned, msg->sync_type);
    if (nexusrv_msg_is_indir_branch(msg))
        CHECK_SCANF(NEXUS_FMT_BTYPE, unsigned, msg->branch_type);
    assert(nexusrv_msg_has_icnt(msg));
    msg->hrepeat = 0;
    CHECK_SCANF(NEXUS_FMT_ICNT, uint32_t, msg->icnt);
    if (nexusrv_msg_has_xaddr(msg))
        CHECK_SCANF(NEXUS_FMT_XADDR, uint64_t, msg->xaddr);
    if (nexusrv_msg_has_hist(msg))
        CHECK_SCANF(NEXUS_FMT_HIST, uint32_t, msg->hist);
    return 0;
handle_rest:
    switch (msg->tcode) {
        case NEXUSRV_TCODE_Ownership:
            CHECK_SCANF(NEXUS_FMT_OWNER_FORMAT, unsigned, msg->ownership_fmt);
            CHECK_SCANF(NEXUS_FMT_OWNER_PRV, unsigned, msg->ownership_priv);
            CHECK_SCANF(NEXUS_FMT_OWNER_V, unsigned, msg->ownership_v);
            CHECK_SCANF(NEXUS_FMT_OWNER_CONTEXT, uint64_t, msg->context);
            break;
        case NEXUSRV_TCODE_Error:
            CHECK_SCANF(NEXUS_FMT_ERROR_ETYPE, unsigned, msg->error_type);
            CHECK_SCANF(NEXUS_FMT_ERROR_ECODE, uint32_t, msg->error_code);
            break;
        case NEXUSRV_TCODE_ResourceFull:
            CHECK_SCANF(NEXUS_FMT_RCODE, unsigned, msg->res_code);
            if (msg->res_code > 2) {
                CHECK_SCANF(NEXUS_FMT_RDATA, uint32_t, msg->res_data);
                break;
            }
            msg->icnt = 0;
            msg->hist = 0;
            msg->hrepeat = 0;
            switch (msg->res_code) {
                case 0:
                    CHECK_SCANF(NEXUS_FMT_ICNT, uint32_t, msg->icnt);
                    break;
                case 1:
                    CHECK_SCANF(NEXUS_FMT_HIST, uint32_t, msg->hist);
                    break;
                case 2:
                    CHECK_SCANF(NEXUS_FMT_HIST, uint32_t, msg->hist);
                    CHECK_SCANF(NEXUS_FMT_HREPEAT, uint32_t, msg->hrepeat);
                    break;
            }
            break;
        case NEXUSRV_TCODE_RepeatBranch:
            CHECK_SCANF(NEXUS_FMT_HREPEAT, uint32_t, msg->hrepeat);
            break;
        case NEXUSRV_TCODE_ProgTraceCorrelation:
            msg->icnt = 0;
            msg->hist = 0;
            msg->hrepeat = 0;
            CHECK_SCANF(NEXUS_FMT_EVCODE, unsigned, msg->stop_code);
            CHECK_SCANF(NEXUS_FMT_CDF, unsigned, msg->cdf);
            CHECK_SCANF(NEXUS_FMT_ICNT, uint32_t, msg->icnt);
            if (msg->cdf == 1)
                CHECK_SCANF(NEXUS_FMT_HIST, uint32_t, msg->hist);
            break;
        case NEXUSRV_TCODE_ICT:
            CHECK_SCANF(NEXUS_FMT_CKSRC, unsigned, msg->cksrc);
            CHECK_SCANF(NEXUS_FMT_CKDF, unsigned, msg->ckdf);
            CHECK_SCANF(NEXUS_FMT_CKDATA0, uint64_t, msg->ckdata0);
            if (msg->ckdf > 0)
                CHECK_SCANF(NEXUS_FMT_CKDATA1, uint64_t, msg->ckdata1);
            break;
    }
    return 0;
}