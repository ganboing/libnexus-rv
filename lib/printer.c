// SPDX-License-Identifer: Apache 2.0
/*
 * printer.c - Nexus-RV message printer
 *
 *  Copyright (C) 2025, Bo Gan <ganboing@gmail.com>
 */

#include <stdio.h>
#include <libnexus-rv/decoder.h>
#include "format.h"

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
            NEXUS_FMT_SRC
            NEXUS_FMT_TIMESTAMP
            NEXUS_FMT_TCODE,
            nexusrv_tcode_str(msg->common.tcode),
            (uint32_t)msg->common.source,
            (uint64_t)msg->common.timestamp,
            msg->common.tcode);
    switch (msg->common.tcode) {
        case NENUSRV_TCODE_OWNERSHIP:
            printed += CHECK_PRINTF(
                    NEXUS_FMT_OWNER_FORMAT
                    NEXUS_FMT_OWNER_PRV
                    NEXUS_FMT_OWNER_V
                    NEXUS_FMT_OWNER_CONTEXT,
                    msg->ownership.format,
                    msg->ownership.prv,
                    msg->ownership.v,
                    msg->ownership.context);
            break;
        case NEXUSRV_TCODE_ERROR:
            printed += CHECK_PRINTF(
                    NEXUS_FMT_ERROR_ETYPE
                    NEXUS_FMT_ERROR_ECODE,
                    msg->error.etype,
                    msg->error.ecode);
            break;
        case NEXUSRV_TCODE_DIRBR:
            printed += CHECK_PRINTF(
                    NEXUS_FMT_ICNT,
                    msg->dirbr.icnt);
            break;
        case NEXUSRV_TCODE_INDIRBR:
            printed += CHECK_PRINTF(
                    NEXUS_FMT_ICNT
                    NEXUS_FMT_BTYPE
                    NEXUS_FMT_UADDR,
                    msg->indirbr.icnt,
                    msg->indirbr.btype,
                    (uint64_t)msg->indirbr.uaddr);
            break;
        case NEXUSRV_TCODE_PROG_SYNC:
            printed += CHECK_PRINTF(
                    NEXUS_FMT_ICNT
                    NEXUS_FMT_SYNC
                    NEXUS_FMT_FADDR,
                    msg->prog_sync.icnt,
                    msg->prog_sync.sync,
                    (uint64_t)msg->prog_sync.faddr);
            break;
        case NEXUSRV_TCODE_DIRBR_SYNC:
            printed += CHECK_PRINTF(
                    NEXUS_FMT_ICNT
                    NEXUS_FMT_SYNC
                    NEXUS_FMT_FADDR,
                    msg->dirbr_sync.icnt,
                    msg->dirbr_sync.sync,
                    (uint64_t)msg->dirbr_sync.faddr);
            break;
        case NEXUSRV_TCODE_INDIRBR_SYNC:
            printed += CHECK_PRINTF(
                    NEXUS_FMT_ICNT
                    NEXUS_FMT_SYNC
                    NEXUS_FMT_BTYPE
                    NEXUS_FMT_FADDR,
                    msg->indirbr_sync.icnt,
                    msg->indirbr_sync.sync,
                    msg->indirbr_sync.btype,
                    (uint64_t)msg->indirbr_sync.faddr);
            break;
        case NEXUSRV_TCODE_INDIRBR_HIST:
            printed += CHECK_PRINTF(
                    NEXUS_FMT_ICNT
                    NEXUS_FMT_BTYPE
                    NEXUS_FMT_UADDR
                    NEXUS_FMT_HIST,
                    msg->indirbr_hist.icnt,
                    msg->indirbr_hist.btype,
                    (uint64_t)msg->indirbr_hist.uaddr,
                    msg->indirbr_hist.hist);
            break;
        case NEXUSRV_TCODE_INDIRBR_HIST_SYNC:
            printed += CHECK_PRINTF(
                    NEXUS_FMT_ICNT
                    NEXUS_FMT_SYNC
                    NEXUS_FMT_BTYPE
                    NEXUS_FMT_FADDR
                    NEXUS_FMT_HIST,
                    msg->indirbr_hist_sync.icnt,
                    msg->indirbr_hist_sync.sync,
                    msg->indirbr_hist_sync.btype,
                    (uint64_t)msg->indirbr_hist_sync.faddr,
                    msg->indirbr_hist_sync.hist);
            break;
        case NEXUSRV_TCODE_RESOURCE_FULL:
            printed += CHECK_PRINTF(
                    NEXUS_FMT_RCODE,
                    msg->resource_full.rcode);
            switch (msg->resource_full.rcode) {
                case 0:
                    printed += CHECK_PRINTF(
                            NEXUS_FMT_ICNT,
                            msg->resource_full.icnt);
                    break;
                case 1:
                    printed += CHECK_PRINTF(
                            NEXUS_FMT_HIST,
                            msg->resource_full.hist);
                    break;
                case 2:
                    printed += CHECK_PRINTF(
                            NEXUS_FMT_HIST
                            NEXUS_FMT_HREPEAT,
                            msg->resource_full.hist,
                            msg->resource_full.hrepeat);
                    break;
                default:
                    printed += CHECK_PRINTF(
                            NEXUS_FMT_RDATA,
                            msg->resource_full.rdata);
                    break;
            }
            break;
        case NEXUSRV_TCODE_REPEAT_BR:
            printed += CHECK_PRINTF(
                    NEXUS_FMT_HREPEAT,
                    msg->repeat_br.hrepeat);
            break;
        case NEXUSRV_TCODE_PROG_CORR:
            printed += CHECK_PRINTF(
                    NEXUS_FMT_EVCODE
                    NEXUS_FMT_ICNT
                    NEXUS_FMT_HIST,
                    msg->prog_corr.evcode,
                    msg->prog_corr.icnt,
                    msg->prog_corr.hist);
            break;
        default:
            break;
    }
    return printed;
}