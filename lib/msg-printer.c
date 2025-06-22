// SPDX-License-Identifier: Apache 2.0
/*
 * msg-printer.c - NexusRV message printer
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <stdio.h>
#include <libnexus-rv/msg-decoder.h>
#include "msg-format.h"

#define CHECK_PRINTF(...)               \
({                                      \
    int rc = fprintf(fp, __VA_ARGS__);  \
    if (rc < 0)                         \
        return rc;                      \
    rc;                                 \
})

int nexusrv_print_msg(FILE *fp, const nexusrv_msg *msg) {
    int printed = 0;
    printed += CHECK_PRINTF(
            "%s"
            NEXUS_FMT_TIMESTAMP
            NEXUS_FMT_TCODE,
            nexusrv_tcode_str(msg->tcode),
            (uint64_t)msg->timestamp,
            msg->tcode);
    if (nexusrv_msg_has_src(msg))
        printed += CHECK_PRINTF(NEXUS_FMT_SRC, msg->src);
    switch (msg->tcode) {
        case NEXUSRV_TCODE_Idle:
            return printed;
        case NEXUSRV_TCODE_Ownership:
        case NEXUSRV_TCODE_Error:
        case NEXUSRV_TCODE_DataAcquisition:
        case NEXUSRV_TCODE_ResourceFull:
        case NEXUSRV_TCODE_RepeatBranch:
        case NEXUSRV_TCODE_ProgTraceCorrelation:
            goto handle_rest;
        default:
            break;
    }
    if (nexusrv_msg_is_sync(msg))
        printed += CHECK_PRINTF(NEXUS_FMT_SYNC, msg->sync_type);
    if (nexusrv_msg_is_indir_branch(msg))
        printed += CHECK_PRINTF(NEXUS_FMT_BTYPE, msg->branch_type);
    if (nexusrv_msg_has_icnt(msg))
        printed += CHECK_PRINTF(NEXUS_FMT_ICNT, msg->icnt);
    if (nexusrv_msg_has_xaddr(msg))
        printed += CHECK_PRINTF(NEXUS_FMT_XADDR, msg->xaddr);
    if (nexusrv_msg_has_hist(msg))
        printed += CHECK_PRINTF(NEXUS_FMT_HIST, msg->hist);
    return printed;
handle_rest:
    switch (msg->tcode) {
        case NEXUSRV_TCODE_Ownership:
            printed += CHECK_PRINTF(
                    NEXUS_FMT_OWNER_FORMAT
                    NEXUS_FMT_OWNER_PRV
                    NEXUS_FMT_OWNER_V
                    NEXUS_FMT_OWNER_CONTEXT,
                    msg->ownership_fmt,
                    msg->ownership_priv,
                    msg->ownership_v,
                    msg->context);
            break;
        case NEXUSRV_TCODE_Error:
            printed += CHECK_PRINTF(
                    NEXUS_FMT_ERROR_ETYPE
                    NEXUS_FMT_ERROR_ECODE,
                    msg->error_type,
                    msg->error_code);
            break;
        case NEXUSRV_TCODE_DataAcquisition:
            printed += CHECK_PRINTF(
                    NEXUS_FMT_DA_IDTAG
                    NEXUS_FMT_DA_DQDATA,
                    msg->idtag,
                    msg->dqdata);
            break;
        case NEXUSRV_TCODE_ResourceFull:
            printed += CHECK_PRINTF(NEXUS_FMT_RCODE, msg->res_code);
            if (msg->res_code > 2) {
                printed += CHECK_PRINTF(NEXUS_FMT_RDATA, msg->res_data);
                break;
            }
            switch (msg->res_code) {
                case 0:
                    printed += CHECK_PRINTF(NEXUS_FMT_ICNT, msg->icnt);
                    break;
                case 1:
                    printed += CHECK_PRINTF(NEXUS_FMT_HIST, msg->hist);
                    break;
                case 2:
                    printed += CHECK_PRINTF(NEXUS_FMT_HIST, msg->hist);
                    printed += CHECK_PRINTF(NEXUS_FMT_HREPEAT, msg->hrepeat);
                    break;
            }
            break;
        case NEXUSRV_TCODE_RepeatBranch:
            printed += CHECK_PRINTF(
                    NEXUS_FMT_HREPEAT,
                    msg->hrepeat);
            break;
        case NEXUSRV_TCODE_ProgTraceCorrelation:
            printed += CHECK_PRINTF(
                    NEXUS_FMT_EVCODE
                    NEXUS_FMT_CDF
                    NEXUS_FMT_ICNT,
                    msg->stop_code,
                    msg->cdf,
                    msg->icnt);
            if (msg->cdf == 1)
                printed += CHECK_PRINTF(NEXUS_FMT_HIST, msg->hist);
            break;
    }
    return printed;
}